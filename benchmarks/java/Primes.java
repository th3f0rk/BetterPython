// Prime sieve benchmark - tests loops and array operations
public class Primes {
    public static int countPrimes(int limit) {
        boolean[] sieve = new boolean[limit + 1];
        for (int i = 0; i <= limit; i++) sieve[i] = true;
        sieve[0] = sieve[1] = false;

        for (int i = 2; i * i <= limit; i++) {
            if (sieve[i]) {
                for (int j = i * i; j <= limit; j += i) {
                    sieve[j] = false;
                }
            }
        }

        int count = 0;
        for (int i = 2; i <= limit; i++) {
            if (sieve[i]) count++;
        }
        return count;
    }

    public static void main(String[] args) {
        int limit = 1000000;
        if (args.length > 0) limit = Integer.parseInt(args[0]);

        long start = System.nanoTime();
        int result = countPrimes(limit);
        long elapsed = (System.nanoTime() - start) / 1000000;

        System.out.println("Primes up to " + limit + ": " + result);
        System.out.println("Time: " + elapsed + " ms");
    }
}
