--[[
    See: https://illumos.org/man/1M/zfs-program
--]]

out = {};

function ze_list(dataset, columns)
    for _,column in pairs(columns) do
        out[column] = zfs.get_prop(dataset, column);
    end
end

args = ... ;

ze_list(args["dataset"], args["columns"]);

results = {};
results["out"] = out;

return results;
