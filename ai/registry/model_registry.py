"""Model registry: candidate → promote / reject / production rollback."""
from __future__ import annotations

import json
import shutil
from dataclasses import asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from ai.learning.training.weight_space import assert_risk_safety_unchanged
from ai.registry.live_guard import LiveMutationError
from ai.registry.model_metadata import ModelMetadata


class ModelRegistry:
    def __init__(self, root: Path):
        self.root = Path(root)
        for sub in ("candidate", "production", "archived", "rejected"):
            (self.root / sub).mkdir(parents=True, exist_ok=True)

    def _candidate_dir(self, name: str, version: str) -> Path:
        return self.root / "candidate" / name / version

    def _production_dir(self, name: str, version: str) -> Path:
        return self.root / "production" / "versions" / name / version

    def _archived_dir(self, name: str, version: str) -> Path:
        return self.root / "archived" / name / version

    def _rejected_dir(self, name: str, version: str) -> Path:
        return self.root / "rejected" / name / version

    def production_manifest_path(self) -> Path:
        return self.root / "production" / "manifest.json"

    def load_production_manifest(self) -> dict[str, Any]:
        path = self.production_manifest_path()
        if not path.exists():
            return {
                "name": "vs-v2",
                "version": None,
                "status": "empty",
                "history": [],
                "models": [],
            }
        return json.loads(path.read_text(encoding="utf-8"))

    def _write_production_manifest(self, manifest: dict[str, Any]) -> None:
        path = self.production_manifest_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")

    def register(self, name: str, version: str, metadata: dict[str, Any]) -> Path:
        dest = self.root / "candidate" / name / version / "metadata.json"
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(metadata, indent=2, sort_keys=True), encoding="utf-8")
        return dest

    def list_versions(self, name: str, *, bucket: str = "candidate") -> list[str]:
        p = self.root / bucket / name
        if bucket == "production":
            p = self.root / "production" / "versions" / name
        if not p.exists():
            return []
        return sorted(x.name for x in p.iterdir() if x.is_dir())

    def list_models(self) -> list[ModelMetadata]:
        out: list[ModelMetadata] = []
        for bucket in ("candidate", "production", "archived", "rejected"):
            base = self.root / bucket
            if bucket == "production":
                base = self.root / "production" / "versions"
            if not base.exists():
                continue
            for name_dir in sorted(base.iterdir()):
                if not name_dir.is_dir():
                    continue
                for ver_dir in sorted(name_dir.iterdir()):
                    if not ver_dir.is_dir():
                        continue
                    meta_path = ver_dir / "metadata.json"
                    model_path = ver_dir / "model.json"
                    metrics: dict[str, float] = {}
                    if model_path.exists():
                        art = json.loads(model_path.read_text(encoding="utf-8"))
                        train = (art.get("metrics") or {}).get("train") or {}
                        metrics = {k: float(v) for k, v in train.items() if isinstance(v, (int, float))}
                    elif meta_path.exists():
                        meta = json.loads(meta_path.read_text(encoding="utf-8"))
                        metrics = {k: float(v) for k, v in (meta.get("metrics") or {}).items()}
                    status = "production" if bucket == "production" else bucket
                    if bucket == "production" and name_dir.name:
                        status = "production"
                    out.append(
                        ModelMetadata(
                            name=name_dir.name,
                            version=ver_dir.name,
                            status=status,
                            metrics=metrics,
                        )
                    )
        return out

    def load_candidate(self, name: str, version: str) -> dict[str, Any]:
        path = self._candidate_dir(name, version) / "model.json"
        if not path.exists():
            raise FileNotFoundError(f"candidate not found: {name}/{version}")
        return json.loads(path.read_text(encoding="utf-8"))

    def reject(self, name: str, version: str, reason: str) -> Path:
        src = self._candidate_dir(name, version)
        if not src.exists():
            raise FileNotFoundError(f"candidate not found: {name}/{version}")
        dest = self._rejected_dir(name, version)
        if dest.exists():
            shutil.rmtree(dest)
        shutil.move(str(src), str(dest))
        reason_path = dest / "rejection.json"
        reason_path.write_text(
            json.dumps(
                {
                    "reason": reason,
                    "rejected_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                    "model_id": name,
                    "version": version,
                },
                indent=2,
                sort_keys=True,
            ),
            encoding="utf-8",
        )
        model_path = dest / "model.json"
        if model_path.exists():
            art = json.loads(model_path.read_text(encoding="utf-8"))
            art["status"] = "rejected"
            art["rejected_reason"] = reason
            model_path.write_text(json.dumps(art, indent=2, sort_keys=True), encoding="utf-8")
        return dest

    def promote(self, name: str, version: str, *, validation: dict[str, Any] | None = None) -> Path:
        """Promote candidate → production. Previous production archived for rollback."""
        src = self._candidate_dir(name, version)
        if not src.exists():
            raise FileNotFoundError(f"candidate not found: {name}/{version}")
        artifact = json.loads((src / "model.json").read_text(encoding="utf-8"))
        assert_risk_safety_unchanged(artifact.get("weights") or {})
        if artifact.get("status") == "live":
            raise LiveMutationError("cannot promote an already-LIVE self-mutating artifact")

        manifest = self.load_production_manifest()
        current_version = manifest.get("version")
        current_name = manifest.get("name") or name

        # Archive current production if present.
        if current_version:
            prod_src = self._production_dir(current_name, str(current_version))
            if prod_src.exists():
                arch = self._archived_dir(current_name, str(current_version))
                if arch.exists():
                    shutil.rmtree(arch)
                shutil.copytree(prod_src, arch)
                history = list(manifest.get("history") or [])
                history.append(
                    {
                        "version": current_version,
                        "archived_at": datetime.now(timezone.utc).strftime(
                            "%Y-%m-%dT%H:%M:%SZ"
                        ),
                    }
                )
                manifest["history"] = history

        dest = self._production_dir(name, version)
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(src, dest)

        art_path = dest / "model.json"
        art = json.loads(art_path.read_text(encoding="utf-8"))
        art["status"] = "production"
        if validation is not None:
            art["validation"] = validation
        art_path.write_text(json.dumps(art, indent=2, sort_keys=True), encoding="utf-8")

        # Remove candidate copy after successful promote (immutable snapshot in production).
        shutil.rmtree(src)

        manifest.update(
            {
                "name": name,
                "version": version,
                "status": "production",
                "promoted_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "models": [{"name": name, "version": version}],
            }
        )
        self._write_production_manifest(manifest)
        (dest / "metadata.json").write_text(
            json.dumps(asdict(ModelMetadata(name=name, version=version, status="production")), indent=2),
            encoding="utf-8",
        )
        return dest

    def rollback(self, *, to_version: str | None = None) -> dict[str, Any]:
        """Roll production back to previous archived version (or explicit to_version)."""
        manifest = self.load_production_manifest()
        name = manifest.get("name") or "vs-v2"
        current = manifest.get("version")
        history = list(manifest.get("history") or [])

        if to_version is None:
            if not history:
                raise ValueError("no archived production version to rollback to")
            to_version = str(history[-1]["version"])

        arch = self._archived_dir(str(name), str(to_version))
        if not arch.exists():
            raise FileNotFoundError(f"archived version not found: {name}/{to_version}")

        # Archive current production before rollback.
        if current:
            prod_src = self._production_dir(str(name), str(current))
            if prod_src.exists():
                bump = self._archived_dir(str(name), str(current))
                if bump.exists():
                    shutil.rmtree(bump)
                shutil.copytree(prod_src, bump)
                history.append(
                    {
                        "version": current,
                        "archived_at": datetime.now(timezone.utc).strftime(
                            "%Y-%m-%dT%H:%M:%SZ"
                        ),
                        "reason": "pre_rollback",
                    }
                )

        dest = self._production_dir(str(name), str(to_version))
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(arch, dest)

        art_path = dest / "model.json"
        if art_path.exists():
            art = json.loads(art_path.read_text(encoding="utf-8"))
            art["status"] = "production"
            art["rolled_back_at"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
            art_path.write_text(json.dumps(art, indent=2, sort_keys=True), encoding="utf-8")

        manifest.update(
            {
                "name": name,
                "version": to_version,
                "status": "production",
                "rolled_back_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
                "history": history,
                "models": [{"name": name, "version": to_version}],
            }
        )
        self._write_production_manifest(manifest)
        return {"name": name, "version": to_version, "previous": current}
