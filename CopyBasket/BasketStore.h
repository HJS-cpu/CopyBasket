#pragma once

#include <string>
#include <vector>

namespace BasketStore {
    void AddFiles(const std::vector<std::wstring>& files);
    std::vector<std::wstring> ReadBasket();
    void WriteBasket(const std::vector<std::wstring>& files);
    void ClearBasket();
    int GetFileCount();
}
