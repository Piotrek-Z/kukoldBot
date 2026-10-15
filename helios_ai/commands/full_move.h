#pragma once

namespace commands {

// Ruch całym robotem - sprawdzenie podsystemów na raz:
//   - ręce: dokładnie tak jak arms_move(),
//   - tułów: krótka sekwencja testowa (pitch przód/tył, roll lewo/prawo, zero),
//   - baza: krótka sekwencja jogu (przód/tył, obrót lewo/prawo - wraca na miejsce).
// Wszystko równolegle, w wątkach.
void full_move();

} // namespace commands
