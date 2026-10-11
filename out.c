struct Point {
  int x;
  int y;
};

union Value {
  int ival;
  short sval;
};

struct Config {
  int enabled;
  int mode;
} default_cfg = {1, 2};

void noop(void) {
  return ;
}

int square(int x) {
  return (x * x);
}

int apply(int (*op)(int), int val) {
  return op(val);
}

short add_short(short s, short si) {
  return (s + si);
}

int deref_sum(int *p1, int **p2) {
  return ((*p1) + (*(*p2)));
}

int main(void) {
  int a = 10;
  int b = 20;
  int uninit_var;
  short s = 5;
  short s2 = 6;
  long l = 30;
  long l2 = 31;
  long long ll = 40;
  long long ll2 = 41;
  int arr[4] = {10, 20, 30, 40};
  int auto_arr[3] = {1, 2, 3};
  struct Point pt = {100, 200};
  struct Point *ppt = &pt;
  union Value val;
  int *p = &arr[1];
  int **pp = &p;
  int (*fn)(int) = square;
  noop();
  int sq = apply(fn, 5);
  short mod_sum = add_short(s, s2);
  int d_sum = deref_sum(p, pp);
  pt.x = 101;
  ppt->y = 202;
  val.ival = 4660;
  val.sval = 42;
  arr[0] = 15;
  (*p) = 25;
  a = uninit_var = 50;
  int u_pos = + a;
  int u_neg = - a;
  int u_not = !a;
  int u_bnot = ~a;
  int *u_addr = &pt.x;
  int u_deref = (*u_addr);
  int arith = (((((a * 2) + 10) - 4) / 2) % 7);
  int rel = ((((a < 100) && (a <= 50)) && (b > 0)) && (b >= 20));
  int eq = ((a == 50) || (b != 20));
  int hex = 42;
  {
    int block_var = 99;
    a = (a + block_var);
  }
  if ((a > 100)) {
    a = (a - 10);
  } else {
    a = (a + 10);
  }
  if ((hex == 42)) {
    hex = (hex + 1);
  }
  int w_count = 3;
  while ((w_count > 0)) {
    w_count = (w_count - 1);
  }
  int dw_count = 0;
  do {
    dw_count = (dw_count + 1);
  } while ((dw_count < 2));
  int for_acc = 0;
  for (int i = 0; (i < 5); i = (i + 1)) {
    if ((i == 1)) {
      continue;
    }
    if ((i == 4)) {
      break;
    }
    for_acc = (for_acc + i);
  }
  for (; (for_acc < 10); for_acc = (for_acc + 1)) {
  }
  int sw_res = 0;
  switch (dw_count) {
    case 1:
      sw_res = 100;
      break;
    case 2:
      sw_res = 200;
      break;
    default:
      sw_res = - 1;
      break;
  }
  goto target_label;
  a = 0;
  target_label:
  return ((((((((((((((((((((((pt.x + ppt->y) + arr[0]) + (*p)) + sq) + hex) + sw_res) + mod_sum) + d_sum) + u_deref) + arith) + rel) + eq) + u_pos) + u_neg) + u_not) + u_bnot) + auto_arr[0]) + val.sval) + l) + l2) + ll) + ll2);
}

