.pragma library

// Explicit surfaces only; clip masks to the scrolling viewport, not its content.
function collect(items, target, viewport) {
    var result = []
    var v = viewport.mapToItem(target, 0, 0)
    for (var i = 0; i < items.length; ++i) {
        var item = items[i]
        if (!item || !item.visible || item.width <= 0 || item.height <= 0) continue
        var p = item.mapToItem(target, 0, 0)
        var x = Math.max(v.x, p.x), y = Math.max(v.y, p.y)
        var right = Math.min(v.x + viewport.width, p.x + item.width)
        var bottom = Math.min(v.y + viewport.height, p.y + item.height)
        if (right > x && bottom > y)
            result.push({x: x, y: y, width: right-x, height: bottom-y,
                radius: x === p.x && y === p.y && right === p.x+item.width && bottom === p.y+item.height ? 16 : 0})
    }
    return result
}

function delegates(view) {
    var result = []
    var children = view.contentItem.children
    for (var i = 0; i < children.length; ++i)
        if (children[i].cardDelegate === true) result.push(children[i])
    return result
}
