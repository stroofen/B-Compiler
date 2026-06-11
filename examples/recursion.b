N 100;


main() {
    extrn callself;
    extrn callself_ptr;
    extrn printf;

    callself(N);
}

callself(n) {
    extrn callself_ptr;
    extrn printf;
    
    printf("callself(%d) ", n);
    if(n <= 0) {
        return;
    }
    callself(n - 1);
}