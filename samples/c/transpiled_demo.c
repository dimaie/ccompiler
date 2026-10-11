/* =========================================================================
 * C Transpiler Demonstration
 * ========================================================================= */

struct Point {
    int x;
    int y;
};

int distance_squared(struct Point *p) {
    return (p->x * p->x) + (p->y * p->y);
}

int main() {
    struct Point pt;
    pt.x = 3;
    pt.y = 4;
    return distance_squared(&pt); // 3^2 + 4^2 = 25
}
