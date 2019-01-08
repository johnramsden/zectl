--[[ See: https://illumos.org/man/1M/zfs-program ]]

function ze_get_prop(dataset, props)
    local properties = {};

    for _,prop in pairs(props) do
        local prop_value = {};
        --[[prop_value.error,]]
        prop_value.value = zfs.get_prop(dataset, prop);
        properties[prop] = prop_value;
    end

    return properties;
end

args = ... ;
out = ze_get_prop(args.beroot, args.columns);

return out;
