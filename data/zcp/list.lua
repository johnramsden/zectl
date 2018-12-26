--[[
    See: https://illumos.org/man/1M/zfs-program
--]]

print("zfs list from lua!")

out = {}

function zedenv_list(dataset)
    for child in zfs.list.children(dataset) do
        out[child] = child
    end
end

args = ...
argv = args["argv"]

dataset = argv[1]
zedenv_list(dataset)

results = {}
results["out"] = out

return "OK"
