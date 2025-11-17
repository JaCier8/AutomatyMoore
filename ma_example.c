#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdlib.h>
#include "ma.h"
#include "memory_tests.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** MAKRA SKRACAJĄCE IMPLEMENTACJĘ TESTÓW **/

// To są możliwe wyniki testu.
#define PASS 0
#define FAIL 1
#define WRONG_TEST 2

// Oblicza liczbę elementów tablicy x.
#define SIZE(x) (sizeof x / sizeof x[0])

#define ASSERT(f)                        \
  do {                                   \
    if (!(f))                            \
      return FAIL;                       \
  } while (0)

#define V(code, where) (((unsigned long)code) << (3 * where))

/** WŁAŚCIWE TESTY **/

void t_one(uint64_t *next_state, uint64_t const *input,
           uint64_t const *old_state, size_t, size_t) {
  next_state[0] = old_state[0] + input[0];
}

void y_one(uint64_t *output, uint64_t const *state, size_t, size_t) {
  output[0] = state[0] + 1;
}

static void t_two(uint64_t *next_state, uint64_t const *input,
                  uint64_t const *old_state, size_t, size_t) {
  next_state[0] = old_state[0] ^ input[0];
}

// Testuje jeden automat wykonujący jakieś dodawania.
static int one(void) {
  const uint64_t q1 = 1, x3 = 3, *y;

  moore_t *a = ma_create_full(64, 64, 64, t_one, y_one, &q1);
  assert(a);

  y = ma_get_output(a);
  ASSERT(y != NULL);
  ASSERT(ma_set_input(a, &x3) == 0);
  ASSERT(y[0] == 2);
  ASSERT(ma_step(&a, 1) == 0);
  ASSERT(y[0] == 5);
  ASSERT(ma_step(&a, 1) == 0);
  ASSERT(y[0] == 8);
  ASSERT(ma_set_input(a, &q1) == 0);
  ASSERT(ma_set_state(a, &x3) == 0);
  ASSERT(y[0] == 4);
  ASSERT(ma_step(&a, 1) == 0);
  ASSERT(y[0] == 5);
  ASSERT(ma_step(&a, 1) == 0);
  ASSERT(y[0] == 6);

  ma_delete(a);
  return PASS;
}

// Testuje dwa automaty tworzące dwubitowy licznik binarny.
static int two(void) {
  uint64_t x = 1;
  const uint64_t *y[2];
  moore_t *a[2];

  a[0] = ma_create_simple(1, 1, t_two);
  a[1] = ma_create_simple(1, 1, t_two);
  assert(a[0]);
  assert(a[1]);

  y[0] = ma_get_output(a[0]);
  y[1] = ma_get_output(a[1]);
  ASSERT(y[0] != NULL);
  ASSERT(y[1] != NULL);

  // Na początku licznik ma wartość 00.
  ASSERT(ma_set_input(a[0], &x) == 0);
  ASSERT(ma_connect(a[1], 0, a[0], 0, 1) == 0);
  ASSERT(y[1][0] == 0 && y[0][0] == 0);

  // Po jednym kroku licznik ma wartość 01.
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 0 && y[0][0] == 1);

  // Po dwóch krokach licznik ma wartość 10.
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 1 && y[0][0] == 0);

  // Po trzech krokach licznik ma wartość 11.
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 1 && y[0][0] == 1);

  // Po czterech krokach licznik ma wartość 00.
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 0 && y[0][0] == 0);
  ASSERT(ma_step(a, 2) == 0);

  // Po pięciu krokach licznik ma wartość 01.
  ASSERT(y[1][0] == 0 && y[0][0] == 1);

  // Po rozłączeniu automatów starczy bit licznika przestaje się zmieniać.
  ASSERT(ma_disconnect(a[1], 0, 1) == 0);
  x = 0;
  ASSERT(ma_set_input(a[1], &x) == 0);
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 0 && y[0][0] == 0);
  ASSERT(ma_step(a, 2) == 0);
  ASSERT(y[1][0] == 0 && y[0][0] == 1);

  ma_delete(a[0]);
  ma_delete(a[1]);
  return PASS;
}

// Testuje reakcję implementacji na niepowodzenie alokacji pamięci.
// Błąd alokacji jest zgłaszany raz. Druga próba powinna się udać.
static unsigned long alloc_fail_test(void) {
  const uint64_t q1 = 1;
  unsigned long visited = 0;
  moore_t *maf, *mas;

  errno = 0;
  if ((maf = ma_create_full(64, 64, 64, t_one, y_one, &q1)) != NULL)
    visited |= V(1, 0);
  else if (errno == ENOMEM &&
           (maf = ma_create_full(64, 64, 64, t_one, y_one, &q1)) != NULL)
    visited |= V(2, 0);
  else
    return visited |= V(4, 0); // To nie powinno się wykonać.

  errno = 0;
  if ((mas = ma_create_simple(1, 1, t_two)) != NULL)
    visited |= V(1, 1);
  else if (errno == ENOMEM && (mas = ma_create_simple(1, 1, t_two)) != NULL)
    visited |= V(2, 1);
  else
    return visited |= V(4, 1); // To nie powinno się wykonać.

  ma_delete(maf);
  ma_delete(mas);

  return visited;
}

// Testuje reakcję implementacji na niepowodzenie alokacji pamięci.
static int memory_test(unsigned long (* test_function)(void)) {
  memory_test_data_t *mtd = get_memory_test_data();

  unsigned fail = 0, pass = 0;
  mtd->call_total = 0;
  mtd->fail_counter = 1;
  while (fail < 3 && pass < 3) {
    mtd->call_counter = 0;
    mtd->alloc_counter = 0;
    mtd->free_counter = 0;
    mtd->function_name = NULL;
    unsigned long visited_points = test_function();
    if (mtd->alloc_counter != mtd->free_counter ||
        (visited_points & 0444444444444444444444UL) != 0) {
      fprintf(stderr,
              "fail_counter %u, alloc_counter %u, free_counter %u, "
              "function_name %s, visited_point %lo\n",
              mtd->fail_counter, mtd->alloc_counter, mtd->free_counter,
              mtd->function_name, visited_points);
      ++fail;
    }
    if (mtd->function_name == NULL)
      ++pass;
    else
      pass = 0;
    mtd->fail_counter++;
  }

  return mtd->call_total > 0 && fail == 0 ? PASS : FAIL;
}







uint64_t popcount64(uint64_t x) {
  uint64_t count = 0;
  while (x) {
      count += x & 1;
      x >>= 1;
  }
  return count;
}

void t_zero(uint64_t *next_state, uint64_t const *input, uint64_t const *old_state, size_t n, size_t s) {
    for (size_t i = 0; i <= s/64; i++) next_state[i] = 0;
  }

  void t_bitcounter(uint64_t *next_state, uint64_t const *input, uint64_t const *old_state, size_t n, size_t s) {
    next_state[0] = 0;
    for (size_t i = 0; i <= n/64; i++) next_state[0] += popcount64(input[i]);
  }

  const uint64_t STATE_MOD = 6765878346298313;
  void refresh_state(uint64_t state[]) {
    state[0] = (state[0] + 2*state[1])%STATE_MOD;
    state[1] = (3*state[0] + state[1])%STATE_MOD;
  }

  /*
    Test tworzy N + 1 automatów simple
    Automaty w tablicy a - mają 0 wejścia i funkcję tranzycji zerująca stan
    Automat counter zlicza bity na wejściu - ma on duuuże wejście, do którego podpinają się automaty a (do niezależnych fragmentów)
    Następnie automaty z tablicy a po kolei zerują swoje wejście (wykonując krok)
  */
  static int counter_test(void) {
    const size_t N = 30;
    const size_t p = 131;
    const size_t c = 257;
    moore_t* a[N];
    for (size_t i = 0; i < N; i++) {
      a[i] = ma_create_simple(0, 124, t_zero);
      assert(a[i]);
    }
    moore_t* counter = ma_create_simple(5000, 30, t_bitcounter);
    const uint64_t *y = ma_get_output(counter);
    assert(counter);
    assert(y);
    ASSERT(y[0] == 0);

    uint64_t state[2];
    state[0] = 89327592387342423;
    state[1] = 90780953242323231;
    size_t perm[N];
    //w miarę losowa permutacja liczb 0, ..., N-1
    perm[0] = 7; for (size_t i = 1; i < N; i++) perm[i] = (perm[i-1] + 11)%N;

    uint64_t expected_ans = 0;
    for (size_t perm_id = 0; perm_id < N; perm_id++) {
      //generujemy losowy stan a[i] i podpinamy jego wyjście (121 bitów) do przedziału wejścia countera
      size_t i = perm[perm_id];
      refresh_state(state);
      ma_set_state(a[i], state);
      ma_connect(counter, (i*p) + c, a[i], 0, 121);
      ma_step(&counter, 1);
      expected_ans += popcount64(state[0]) + popcount64(state[1]);
      ASSERT(y[0] == expected_ans);
    }

    //printf("\n \nReshuffling \n \n");
    //w miarę losowa permutacja liczb 0, ..., N-1
    perm[0] = 13; for (size_t i = 1; i < N; i++) perm[i] = (perm[i-1] + 17)%N;

    for (size_t perm_id = 0; perm_id < N; perm_id++) {
      printf("doszedłem\n");
      size_t i = perm[perm_id];

      //czyscimy (step dla a[i] oznacza wyzerowanie outputu)
      uint64_t const* curr_y = ma_get_output(a[i]);
      assert(curr_y);
      expected_ans -= popcount64(curr_y[0]) + popcount64(curr_y[1]);
      ma_step(&(a[i]), 1);

      ma_step(&counter, 1);
      ASSERT(y[0] == expected_ans);
      printf("%ld",perm_id);
    }


    for (size_t i = 0; i < N; i++) ma_delete(a[i]);
    ma_delete(counter);
    printf("PASS\n");
    return PASS;
  }


  #define BITC_TO_BYTEC(x) ((x+7)/8)
#define BITC_TO_64C(x) ((x+63)/64)

void output_simple(uint64_t *output, uint64_t const *state, size_t m, size_t s) {
	(void)m, (void)s;
	if (state[0] & 1)
		memset(output, -1, BITC_TO_BYTEC(m));
	else
		memset(output,  0, BITC_TO_BYTEC(m));
}

void flip(uint64_t *next_state, uint64_t const *input,
          uint64_t const *state, size_t n, size_t s) {
	(void)input, (void)n, (void)s;
	next_state[0] = !state[0];
}

void my_identity(uint64_t *output, uint64_t const *state, size_t m, size_t s) {
	(void)m;
	memcpy(output, state, BITC_TO_BYTEC(s));
}

void mirror_input(uint64_t *next_state, uint64_t const *input,
                  uint64_t const *state, size_t n, size_t s) {
	(void)state, (void)n, (void)s;
	memcpy(next_state, input, BITC_TO_BYTEC(n));
}

static void pad_with_zeros(uint64_t *arr, size_t size) {
	if (size % 64)
		arr[size / 64] &= ((uint64_t)1 << (size % 64)) - 1;
}

static bool bitset_equal(uint64_t *a, uint64_t *b, size_t s) {
	pad_with_zeros(a, s);
	pad_with_zeros(b, s);
	return !memcmp(a, b, BITC_TO_BYTEC(s));
}

static void bitset_or(uint64_t *res, uint64_t *a, uint64_t *b, size_t s) {
	for (size_t i = 0; i < BITC_TO_64C(s); ++i)
		res[i] = a[i] | b[i];
	pad_with_zeros(res, s);
}

static void bitset_xor(uint64_t *res, uint64_t *a, uint64_t *b, size_t s) {
	for (size_t i = 0; i < BITC_TO_64C(s); ++i)
		res[i] = a[i] ^ b[i];
	pad_with_zeros(res, s);
}

static void set_bit(uint64_t *arr, size_t i, bool value) {
	arr[i / 64] |= ((uint64_t)value << (i % 64));
	arr[i / 64] &= ~((uint64_t)!value << (i % 64));
}

static size_t rd(size_t a, size_t b) {
	return a + (size_t)rand() % (b - a + 1);
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void print_bits(uint64_t *arr, size_t s) {
	for (size_t i = 0; i < s; ++i)
		fputc('0' + ((arr[i / 64] >> (i % 64)) & 1), stderr);
	fputc('\n', stderr);
}

#define PRINT_BITS(comment, x, len) do if (len <= 100) {               \
	fprintf(stderr, "%s:\n", comment);                                 \
	print_bits(x, len);                                                \
} while(0)

#define INFO(x...) if (PRINT_INFO) fprintf(stderr, x)

#define CALL(func) do {                                                \
	int _r = func;                                                     \
	if (!_r) break;                                                    \
	fprintf(stderr, "%s at %s:%d: %s\n", #func, __FILE__, __LINE__,    \
	                                     strerror(errno));             \
	return FAIL;										               \
} while(0)

static bool PRINT_INFO = 0;

uint64_t *zero_array, *connected, *connected_to_1, *buff, *buff2, *buff3;
static void conn_allocate(size_t s) {
	zero_array     = (uint64_t*)malloc(s);
	memset(zero_array, 0, s);
	connected      = (uint64_t*)malloc(s);
	connected_to_1 = (uint64_t*)malloc(s);
	buff           = (uint64_t*)malloc(s);
	buff2          = (uint64_t*)malloc(s);
	buff3          = (uint64_t*)malloc(s);
}

static void conn_free() {
	free(zero_array    );
	free(connected     );
	free(connected_to_1);
	free(buff          );
	free(buff2         );
	free(buff3         );
}

static uint64_t val_source[2] = {0, 1};
static moore_t *create_source(size_t *size, size_t max_size, bool value) {
	*size = rd(1, max_size);
	moore_t *m = ma_create_full(0, *size, 1, flip, output_simple, &val_source[value]);
	assert(m);
	return m;
}

static moore_t *create_main(size_t *size, size_t max_size, const uint64_t **output) {
	*size = rd(1, max_size);
	moore_t *m = ma_create_full(*size, *size, *size, mirror_input, my_identity, zero_array);
	assert(m);
	ma_set_input(m, zero_array);
	*output = ma_get_output(m);
	return m;
}

typedef struct conn_test_config {
	size_t run_count;
	size_t query_count;
	size_t max_size;
} conn_test_config_t;

static const conn_test_config_t conn_tests[] = {
	{100, 100000, 10},
	{100, 100000, 100},
	{100, 1000, 10000},
	{10, 100, 1000000},
};

static int run_connection_test(size_t run_id, size_t query_count, size_t max_size) {
	size_t s[2], mains;
	moore_t *m[3], *main;
	const uint64_t *out;

	m[0] = create_source(&s[0], max_size, 0);
	m[1] = create_source(&s[1], max_size, 1);
	main = m[2] = create_main(&mains, max_size, &out);

	memset(connected, 0, BITC_TO_BYTEC(mains));
	memset(connected_to_1, 0, BITC_TO_BYTEC(mains));

	for (size_t q = 1; q <= query_count; ++q) {
		INFO("Query %lu: ", q);
		int action = rd(0, 29);

		size_t lmain = rd(0, mains - 1);
		if (10 <= action) {
			int source = rd(0, 1);
			size_t lsource = rd(0, s[source] - 1);
			size_t num = rd(1, MIN(s[source] - lsource, mains - lmain));

			INFO("connecting outputs %lu-%lu from m%d to inputs %lu-%lu\n",
					lsource, lsource + num - 1, source, lmain, lmain + num - 1);

			CALL(ma_connect(main, lmain, m[source], lsource, num));

			for (size_t i = 0; i < num; ++i) {
				set_bit(connected, lmain + i, 1);
				set_bit(connected_to_1, lmain + i, source);
			}
		} else if (1 <= action) {
			size_t num = rd(1, mains - lmain);

			INFO("disconnecting inputs %lu-%lu\n", lmain, lmain + num - 1);

			for (size_t i = 0; i < num; ++i) {
				set_bit(connected, lmain + i, 0);
				set_bit(connected_to_1, lmain + i, 0);
			}
			CALL(ma_disconnect(main, lmain, num));
			ma_set_input(main, zero_array);
		} else {
			int mid = rd(0, 2);
			INFO("deleting and creating machine %d\n", mid);
			ma_delete(m[mid]);
			if (mid < 2) {
				m[mid] = create_source(&s[mid], max_size, mid);
				if (mid == 1) {
					bitset_xor(connected, connected, connected_to_1, mains);
					memset(connected_to_1, 0, BITC_TO_BYTEC(mains));
				} else {
					memcpy(connected, connected_to_1, BITC_TO_BYTEC(mains));
				}
			} else {
				main = m[mid] = create_main(&mains, max_size, &out);
				memset(connected, 0, BITC_TO_BYTEC(mains));
				memset(connected_to_1, 0, BITC_TO_BYTEC(mains));
			}
			ma_set_input(main, zero_array);
		}

		// checking if the new connections/disconnections were handled correctly

		uint64_t *output = buff, *inverted = buff2, *ored = buff3;

		CALL(ma_step(&main, 1));    // we update values on main
		memcpy(output, out, BITC_TO_BYTEC(mains));

		CALL(ma_step(m, 2));	    // we flip the values on source machines
		CALL(ma_step(m, 3));		// we update main again and flip the values back
		memcpy(inverted, out, BITC_TO_BYTEC(mains));

		bitset_or(ored, output, inverted, mains);

		if (!bitset_equal(ored, connected, mains) ||
				!bitset_equal(output, connected_to_1, mains)) {
			fprintf(stderr, "Failed on run %lu, query %lu\n", run_id, q);
			PRINT_BITS("Which inputs should be connected", connected, mains);
			PRINT_BITS("Which inputs libma says are connected", ored, mains);
			PRINT_BITS("Which inputs should be connected to m1", connected_to_1, mains);
			PRINT_BITS("Which inputs libma says are connected to m1", output, mains);
			PRINT_BITS("Which inputs libma says are connected to m0", inverted, mains);
			for (int i = 0; i < 3; ++i)
				ma_delete(m[i]);
			return FAIL;
		}
	}

	for (int i = 0; i < 3; ++i)
		ma_delete(m[i]);
	return PASS;
}

#ifndef SIZE
#define SIZE(X) (sizeof (x) / sizeof (x[0]))
#endif

static int connection_stress(void) {
	srand(~2137);
	if (getenv("PRINT_INFO"))
		PRINT_INFO = (bool)atol(getenv("PRINT_INFO"));
	size_t max_size = 0;
	for (size_t i = 0; i < SIZE(conn_tests); ++i)
		max_size = MAX(max_size, conn_tests[i].max_size);

	conn_allocate(BITC_TO_BYTEC(max_size));

	for (size_t group = 0; group < SIZE(conn_tests); ++group) {
		conn_test_config_t cfg = conn_tests[group];
		fprintf(stderr, "Running test group %lu – %lu runs, %lu queries per run, %lu max signal count\n", group + 1, cfg.run_count, cfg.query_count, cfg.max_size);
		for (size_t run = 0; run < cfg.run_count; ++run) {
			INFO("Run %lu\n\n", run);
			if (run_connection_test(run, cfg.query_count, cfg.max_size) == FAIL)
				return FAIL;
		}
	}

	conn_free();
	return PASS;
}


// Testuje reakcję implementacji na niepowodzenie alokacji pamięci.
static int memory(void) {
  memory_tests_check();
  return memory_test(alloc_fail_test);
}

/** URUCHAMIANIE TESTÓW **/

typedef struct {
  char const *name;
  int (*function)(void);
} test_list_t;

#define TEST(t) {#t, t}

static const test_list_t test_list[] = {
  TEST(one),
  TEST(two),
  TEST(memory),
  TEST(counter_test),
  TEST(connection_stress)
};

static int do_test(int (*function)(void)) {
  int result = function();
  puts("quite long magic string");
  return result;
}

int main(int argc, char *argv[]) {
  if (argc == 2)
    for (size_t i = 0; i < SIZE(test_list); ++i)
      if (strcmp(argv[1], test_list[i].name) == 0)
        return do_test(test_list[i].function);

  fprintf(stderr, "Użycie:\n%s nazwa_testu\n", argv[0]);
  return WRONG_TEST;
}
