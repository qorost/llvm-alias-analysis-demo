struct Foo {
    int x;
    int y;
};


int foo(struct Foo *f) {
    struct Foo *pf = f;
    struct Foo *ppf = pf;
    return ppf->x;
}

int main() {

    struct Foo f;
    f.x = 1;
    f.y = 2;

    foo(&f);

}
