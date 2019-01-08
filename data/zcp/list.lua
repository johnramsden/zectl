--[[
    See: https://illumos.org/man/1M/zfs-program
--]]

function ze_list(root, columns)
    local bootenvs = {};
    for child in zfs.list.children(root) do
        local child_props = {};
        for _,column in pairs(columns) do
            child_props[column] = zfs.get_prop(child, column);
        end
        bootenvs[child] = child_props
    end
    return bootenvs;
end

args = ... ;

out = {};
out.bootenvs = ze_list(args.beroot, args.columns);

return out;
