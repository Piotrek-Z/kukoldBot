#pragma once

#include <functional>
#include <string>

namespace commands {

void stats();

// Buduje pełny raport statusu jako string (dokładnie to, co stats()
// drukuje na konsolę). Wolne (odpytuje moduły robota) - nie wołać z UI.
std::string statsReport();

// Hook: wołany z gotowym raportem na końcu każdego stats().
// UWAGA: wywołanie przychodzi z odłączonego wątku komendy - listener musi
// być bezpieczny wątkowo. Ustawić RAZ na starcie programu (brak = cisza).
void setStatsListener(std::function<void(const std::string&)> listener);

} // namespace commands
