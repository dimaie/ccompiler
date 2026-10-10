int square(int x) { return x * x; }

int apply(int (*op)(int), int val) { return op(val); }

int main(void) {
  int arr[3] = {10, 20, 30};
  int *p = &arr[1];
  *p = 42;

  int (*fn)(int) = square;
  int res = apply(fn, 5);

  return *p + res;
}
