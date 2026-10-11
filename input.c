/* =========================================================================
 * Comprehensive C test file demonstrating every grammar construction
 * supported by the compiler front-end and code generation backends.
 * ========================================================================= */

/* 1. Structure definition (standalone) */
struct Point {
  int x;
  int y;
};

/* 2. Union definition (standalone) */
union Value {
  int ival;
  short sval;
};

/* 3. Combined struct definition and variable declaration with initializer */
struct Config {
  int enabled;
  int mode;
} default_cfg = {1, 2};

/* 4. Function definitions: void return and void parameter list */
void noop(void) {
  ;
  return;
}

/* 5. Function definition with single parameter */
int square(int x) {
  return x * x;
}

/* 6. Function pointer as parameter */
int apply(int (*op)(int), int val) {
  return op(val);
}

/* 7. Parameter types with short modifiers */
short add_short(short s, short int si) {
  return s + si;
}

/* 8. Pointer and multi-level pointer parameters */
int deref_sum(int *p1, int **p2) {
  return *p1 + **p2;
}

/* 9. Main function demonstrating every statement, expression, and operator */
int main(void) {
  /* Variable declarations: types, modifiers, uninitialized and initialized */
  int a = 10;
  int b = 20;
  int uninit_var;
  short s = 5;
  short int s2 = 6;
  long l = 30;
  long int l2 = 31;
  long long ll = 40;
  long long int ll2 = 41;

  /* Arrays: explicitly sized and inferred size initializers */
  int arr[4] = {10, 20, 30, 40};
  int auto_arr[] = {1, 2, 3};

  /* Struct and union variable declarations and initializers */
  struct Point pt = {100, 200};
  struct Point *ppt = &pt;
  union Value val;

  /* Pointers, multi-level pointers, and function pointer declarations */
  int *p = &arr[1];
  int **pp = &p;
  int (*fn)(int) = square;

  /* Function calls: void, function pointer, and multi-param functions */
  noop();
  int sq = apply(fn, 5);
  short mod_sum = add_short(s, s2);
  int d_sum = deref_sum(p, pp);

  /* Member access: direct (dot '.') and indirect (arrow '->') */
  pt.x = 101;
  ppt->y = 202;
  val.ival = 0x1234;
  val.sval = 42;

  /* Array element assignment and pointer dereference assignment */
  arr[0] = 15;
  *p = 25;

  /* Chained assignment */
  a = uninit_var = 50;

  /* Unary operators: +, -, !, ~, &, * */
  int u_pos = +a;
  int u_neg = -a;
  int u_not = !a;
  int u_bnot = ~a;
  int *u_addr = &pt.x;
  int u_deref = *u_addr;

  /* Binary operators: arithmetic (+, -, *, /, %) */
  int arith = (a * 2 + 10 - 4) / 2 % 7;

  /* Relational (<, <=, >, >=) and equality (==, !=) operators */
  int rel = (a < 100) && (a <= 50) && (b > 0) && (b >= 20);
  int eq = (a == 50) || (b != 20);

  /* Hexadecimal integer literal */
  int hex = 0x2A;

  /* Compound statement / nested block scope */
  {
    int block_var = 99;
    a = a + block_var;
  }

  /* Empty statement */
  ;

  /* If / Else conditional statements */
  if (a > 100) {
    a = a - 10;
  } else {
    a = a + 10;
  }

  if (hex == 42) {
    hex = hex + 1;
  }

  /* While iteration loop */
  int w_count = 3;
  while (w_count > 0) {
    w_count = w_count - 1;
  }

  /* Do-while iteration loop */
  int dw_count = 0;
  do {
    dw_count = dw_count + 1;
  } while (dw_count < 2);

  /* For loop with declaration init, continue, and break statements */
  int for_acc = 0;
  for (int i = 0; i < 5; i = i + 1) {
    if (i == 1) {
      continue;
    }
    if (i == 4) {
      break;
    }
    for_acc = for_acc + i;
  }

  /* For loop with expression / empty init and empty statement body */
  for (; for_acc < 10; for_acc = for_acc + 1) {
    ;
  }

  /* Switch-case statement with case clauses and default clause */
  int sw_res = 0;
  switch (dw_count) {
    case 1:
      sw_res = 100;
      break;
    case 2:
      sw_res = 200;
      break;
    default:
      sw_res = -1;
      break;
  }

  /* Goto statement and label statement */
  goto target_label;
  a = 0;

target_label:
  /* Return statement with complex expression combining results */
  return pt.x + ppt->y + arr[0] + *p + sq + hex + sw_res + mod_sum + d_sum + u_deref + arith + rel + eq + u_pos + u_neg + u_not + u_bnot + auto_arr[0] + val.sval + l + l2 + ll + ll2;
}
