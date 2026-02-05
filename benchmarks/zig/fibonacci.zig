// Fibonacci benchmark - tests recursive function calls
const std = @import("std");

fn fib(n: i64) i64 {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    const args = try std.process.argsAlloc(std.heap.page_allocator);
    defer std.process.argsFree(std.heap.page_allocator, args);

    var n: i64 = 35;
    if (args.len > 1) {
        n = try std.fmt.parseInt(i64, args[1], 10);
    }

    var timer = try std.time.Timer.start();
    const result = fib(n);
    const elapsed = timer.read() / std.time.ns_per_ms;

    try stdout.print("fib({d}) = {d}\n", .{ n, result });
    try stdout.print("Time: {d} ms\n", .{elapsed});
}
