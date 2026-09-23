# Benchmark adapter for the unmodified GLNS.jl checkout passed in ARGS[1].
# Replace only its output callback to capture unrounded timing and the tour;
# the solver, RNG, parser, defaults, and optimization routines are unchanged.
include(joinpath(ARGS[1], "src", "GLNS.jl"))
using Random

@eval GLNS begin
    const benchmark_result = Ref{Any}(nothing)
    function print_summary(lowest::Tour, timer::Float64, member::Array{Int64,1},
                           param::Dict{Symbol,Any})
        benchmark_result[] = (cost=lowest.cost, seconds=timer,
                              tour=copy(lowest.tour), timeout=param[:timeout],
                              budget_met=param[:budget_met])
        return nothing
    end
end

function main()
    println("ready")
    flush(stdout)
    for line in eachline(stdin)
        args = split(line, '\t')
        args[1] == "quit" && break
        path = String(args[2])
        if args[1] == "parse"
            n, m, sets, dist, member = GLNS.read_file(path)
            dc = UInt64(0)
            sc = UInt64(0)
            for i in 1:n, j in 1:n
                dc = dc * UInt64(1099511628211) + UInt64(dist[i, j])
            end
            for set in sets, v in set
                sc = sc * UInt64(31) + UInt64(v)
            end
            println(join(("parse", n, m, dc, sum(UInt64.(member)), sc), '\t'))
        elseif args[1] == "solve"
            # Collect garbage from earlier calls outside the measurement.
            # Collections triggered by this solve remain inside its timer.
            GC.gc()
            Random.seed!(parse(UInt64, args[3]))
            GLNS.benchmark_result[] = nothing
            begin_ns = time_ns()
            GLNS.solver(path; mode="default", verbose=-1)
            call_seconds = (time_ns() - begin_ns) / 1.0e9
            result = GLNS.benchmark_result[]
            result === nothing && error("solver did not report a result")
            println(join(("solve", result.cost, result.seconds, call_seconds,
                          Int(result.timeout), Int(result.budget_met),
                          join(result.tour .- 1, ',')), '\t'))
        else
            error("unknown benchmark command")
        end
        flush(stdout)
    end
end
main()
