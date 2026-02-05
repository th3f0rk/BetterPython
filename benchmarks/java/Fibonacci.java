// Fibonacci benchmark - tests recursive function calls
public class Fibonacci {
    public static long fib(int n) {
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
    }

    public static void main(String[] args) {
        int n = 35;
        if (args.length > 0) n = Integer.parseInt(args[0]);

        long start = System.nanoTime();
        long result = fib(n);
        long elapsed = (System.nanoTime() - start) / 1000000;

        System.out.println("fib(" + n + ") = " + result);
        System.out.println("Time: " + elapsed + " ms");
    }
}
