// Prime sieve benchmark - tests loops and array operations
const std = @import("std");

fn countPrimes(allocator: std.mem.Allocator, limit: usize) !usize {
    var sieve = try allocator.alloc(bool, limit + 1);
    defer allocator.free(sieve);

    for (sieve) |*s| s.* = true;
    sieve[0] = false;
    sieve[1] = false;

    var i: usize = 2;
    while (i * i <= limit) : (i += 1) {
        if (sieve[i]) {
            var j = i * i;
            while (j <= limit) : (j += i) {
                sieve[j] = false;
            }
        }
    }

    var count: usize = 0;
    for (sieve) |s| {
        if (s) count += 1;
    }
    return count;
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    const args = try std.process.argsAlloc(std.heap.page_allocator);
    defer std.process.argsFree(std.heap.page_allocator, args);

    var limit: usize = 1000000;
    if (args.len > 1) {
        limit = try std.fmt.parseInt(usize, args[1], 10);
    }

    var timer = try std.time.Timer.start();
    const result = try countPrimes(std.heap.page_allocator, limit);
    const elapsed = timer.read() / std.time.ns_per_ms;

    try stdout.print("Primes up to {d}: {d}\n", .{ limit, result });
    try stdout.print("Time: {d} ms\n", .{elapsed});
}
