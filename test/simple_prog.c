struct Foo {
    int x;
    int y;
};


int foo(struct Foo *f) {
    struct Foo *pf = f;
    struct Foo *ppf = pf;
    return ppf->x;
}


int *bar() {

    int i = 0;
    return &i;

}

int* uafdemo() {

    int *i = bar();

    return *i;

}

int main() {

    struct Foo f;
    f.x = 1;
    f.y = 2;

    foo(&f);


    int *tmp = uafdemo();

    printf("demo tmp = %d",*tmp);

}
