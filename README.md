# Automaty Moore'a w C

To jest moja implementacja biblioteki do symulacji automatów Moore'a, napisana w czystym C na zaliczenie z Programowania Obiektowego.

W skrócie: biblioteka jest ładowana dynamicznie (`libma.so`) i pozwala na tworzenie wielu automatów, dynamiczne łączenie ich wejść z wyjściami innych automatów oraz synchroniczne "stepowanie" (wykonywanie kroku) całej sieci na raz.

## Główne Ficzery / Ciekawostki Implementacyjne

Ten projekt to nie tylko prosta implementacja. Jest tu kilka ciekawych rozwiązań, z których jestem zadowolony:

### 1. Zarządzanie Pamięcią na Poziomie Bitów

Żadnego marnowania miejsca. Cały stan automatu, jego wejścia i wyjścia są trzymane jako tablice `uint64_t`. Wszystkie operacje (kopiowanie, ustawianie bitów) działają bezpośrednio na tych tablicach przy użyciu masek i przesunięć bitowych. Dzięki temu automaty mogą mieć np. 1000 wejść, a zużyją tylko `ceil(1000/64)` uintów, zamiast tablicy 1000 booli czy charów.

### 2. Dynamiczny Graf Połączeń

Automaty można dynamicznie łączyć (`ma_connect`) i rozłączać (`ma_disconnect`) w dowolnym momencie.

**Jak to działa?**
* Każdy automat (`moore_t`) ma tablicę `origins`, która dla *każdego* swojego bitu wejściowego przechowuje wskaźnik do automatu wyjściowego i numer bitu wyjściowego, z którego czerpie sygnał.
* Dodatkowo, każdy automat trzyma listę powiązaną (`outList_t` z `lista.c`), która przechowuje wskaźniki do wszystkich automatów, które są podpięte do *jego* wyjść.
* Dzięki temu przy usuwaniu automatu (`ma_delete`) mogę szybko przejść po tej liście i "poinformować" wszystkie automaty "słuchające", że ich wejście właśnie stało się "wiszące" (ustawiając ich `origin` na `NULL`).

### 3. Prawdziwie Synchroniczny Krok

Najważniejsza funkcja, `ma_step`, przyjmuje całą tablicę automatów i wykonuje jeden "takt" zegara dla wszystkich naraz. Jest to kluczowe dla poprawnego działania. Implementacja gwarantuje to, działając w dwóch fazach:

1.  **Faza 1 (Obliczanie):** Dla każdego automatu w tablicy, funkcja najpierw kopiuje sygnały z *poprzednich* wyjść innych automatów do *jego* bufora wejściowego. Następnie wywołuje jego funkcję przejścia (`transition`), która oblicza *nowy* stan na podstawie tych wejść i *starego* stanu. Ten nowy stan jest zapisywany w `new_state`.
2.  **Faza 2 (Zatwierdzenie):** Dopiero gdy *wszystkie* nowe stany są obliczone, druga pętla kopiuje `new_state` do `state` i wywołuje funkcje wyjścia (`output_function`), które aktualizują bity wyjściowe na podstawie tych nowych stanów.

To zapobiega "wyścigom" i gwarantuje, że stan automatu `A` w kroku `t+1` zależy od stanu automatu `B` z kroku `t`, a nie `t+1`.

### 4. Hardkorowe Testowanie Pamięci

To chyba najciekawsza część. Zgodnie z wymaganiami, projekt zawiera specjalny moduł `memory_tests.c`.

* Przy linkowaniu, podmieniane są standardowe funkcje `malloc`, `free`, `calloc` itd. na moje własne wrappery (`__wrap_malloc`, `__wrap_free`).
* Te wrappery liczą każdą alokację i zwolnienie.
* Główna funkcja testująca (`memory_test` w `ma_example.c`) uruchamia inne testy (np. `alloc_fail_test`) w pętli.
* W każdej iteracji pętli, symuluje jeden błąd alokacji (`ENOMEM`) w innym miejscu (np. "piąta alokacja w tym teście ma się nie udać").
* Biblioteka `libma` musi poprawnie obsłużyć ten błąd (zwrócić `NULL` i ustawić `errno`), nie wyciec pamięci i pozostawić wszystkie struktury w spójnym stanie, aby kolejna próba (w następnej iteracji pętli) mogła się powieść.
* To jest prawdziwy stress-test odporności na błędy alokacji.

---

## Struktura Projektu

* `ma.h`: Oficjalny plik nagłówkowy biblioteki. Definiuje API.
* `ma.c`: Serce biblioteki. Zawiera definicję struktury `moore_t` oraz implementację wszystkich funkcji `ma_...`.
* `lista.c`: Mała implementacja listy powiązanej, używana przez `ma.c` do śledzenia grafu połączeń.
* `memory_tests.h` / `memory_tests.c`: Moduł do testowania pamięci. Implementuje wrappery na `malloc` i `free` do symulowania błędów alokacji.
* `ma_example.c`: Zbiór przykładowych testów (w tym `one`, `two`, `memory`, `connection_stress`) dostarczony z zadaniem.
* `wszystkieTesty.c`: Mój główny plik testowy, który łączy wszystkie inne testy w jeden plik wykonywalny.
* `testerki.c` / `toNawetDziała.c`: Pliki robocze z dodatkowymi, niestandardowymi testami.
* `CMakeLists.txt` / `Makefile`: Dwa różne systemy budowania.

---

## Budowanie i Testowanie

W repozytorium panuje lekki bałagan, więc są dwie drogi budowania:

### 1. Metoda `Makefile` (Zalecana)

W repozytorium znajduje się `Makefile`.

* Aby zbudować bibliotekę dynamiczną:
    ```bash
    make libma.so
    ```
* Aby usunąć pliki obiektowe:
    ```bash
    make clean
    ```

### 2. Metoda `CMake` (Dla CLion)

Projekt zawiera również `CMakeLists.txt` skonfigurowany pod CLion. Buduje on jeden plik wykonywalny `AutomatyMoore`, który zawiera w sobie wszystkie testy.

### Uruchamianie Testów

Po zbudowaniu projektu za pomocą `CMake`, możesz uruchomić konkretne testy, podając ich nazwy jako argument programu. Funkcjonalność ta jest zaimplementowana w `main` w pliku `wszystkieTesty.c`.

```bash
# Przykładowe uruchomienie testów z builda CMake
./cmake-build-debug/AutomatyMoore one
./cmake-build-debug/AutomatyMoore two
./cmake-build-debug/AutomatyMoore custom
./cmake-build-debug/AutomatyMoore disruptor

# Uruchomienie testu pamięci
./cmake-build-debug/AutomatyMoore memory

# Uruchomienie stress-testu połączeń
./cmake-build-debug/AutomatyMoore connection_stress
