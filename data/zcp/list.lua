--[[
    See: https://illumos.org/man/1M/zfs-program
--]]

out = {}

function ze_list(dataset)
    for child in zfs.list.children(dataset) do
        out[child] = child
    end
end

args = ...
dataset = args["pool"]

ze_list(dataset)

results = {}
results["out"] = out

return results
