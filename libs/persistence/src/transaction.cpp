#include "mr/persistence/transaction.hpp"
namespace mr {
void Transaction::commit() { for (auto& e : batch_) backend_.store(e); batch_.clear(); }
}