N 10;

main() {
    extrn N;
    extrn fib;
    extrn printf;
    
    auto result;
    result = fib(N);
    printf("Fibonacci of 10 = %d", result);
}

fib(x) {
    auto a, b, i, sum;
    a = 0;
    b = 1;
    i = 1;
    sum = 0;

    while(i++ < x) {
        sum = a + b;
        a = b;
        b = sum;
    }

    return sum;
}