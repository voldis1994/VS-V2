#!/usr/bin/env python3
import argparse, subprocess, sys
p=argparse.ArgumentParser(); p.add_argument('file'); a=p.parse_args()
sys.exit(subprocess.call(['./build/market-core','--mode','REPLAY','--replay',a.file]))
