import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DamageDemo

ApplicationWindow {
    id: win
    width: 1440
    height: 860
    minimumWidth: 1080
    minimumHeight: 680
    visible: true
    title: "GUI 损伤跟踪器"
    color: "#0a0f1a"

    readonly property color panelColor: "#111827"
    readonly property color raisedColor: "#172033"
    readonly property color borderColor: "#263247"
    readonly property color textColor: "#f3f6fb"
    readonly property color mutedColor: "#8996aa"
    readonly property color accentColor: "#6d7cff"
    property var sel: ({})
    property int damageHistoryDuration: 200
    readonly property int detectedRefreshRate: win.screen && win.screen.refreshRate > 0
                                               ? Math.round(win.screen.refreshRate) : 60
    property int sceneRefreshRate: detectedRefreshRate
    property bool treeDragging: false
    property int treeDragId: 0
    property string treeDragName: ""
    property int treeDropTargetId: 0
    property int treeDropParentId: 0
    property int treeDropBeforeId: 0
    property int treeDropMode: 0
    property real treePressX: 0
    property real treePressY: 0
    property real treeGhostX: 0
    property real treeGhostY: 0
    property string treeDropHint: ""
    property bool demoPausedByInteraction: false

    function beginUserInteraction() {
        if (scene.demoRunning) {
            demoPausedByInteraction = true
            scene.demoRunning = false
        }
    }

    function endUserInteraction() {
        if (demoPausedByInteraction) {
            demoPausedByInteraction = false
            scene.demoRunning = true
        }
    }
    palette.window: color
    palette.windowText: textColor
    palette.base: raisedColor
    palette.alternateBase: panelColor
    palette.text: textColor
    palette.button: raisedColor
    palette.buttonText: textColor
    palette.highlight: accentColor
    palette.highlightedText: "#ffffff"
    palette.mid: borderColor

    DemoScene {
        id: scene
        refreshRate: win.sceneRefreshRate
        Component.onCompleted: {
            loadDemoScene("occlusion")
            win.sel = scene.selectedProps
        }
    }

    Connections {
        target: scene
        function onSelectedPropsChanged() {
            win.sel = scene.selectedProps
        }
        function onSceneChanged() {
            if (!win.treeDragging)
                win.resetTreeDrag()
        }
    }

    function num(v, fallback) {
        const n = Number(v)
        return Number.isFinite(n) ? n : (fallback ?? 0)
    }

    function typeLabel(type) {
        if (type === "Geometry")
            return "几何节点"
        if (type === "Backdrop")
            return "背景采样节点"
        if (type === "Transform")
            return "变换节点"
        return "分组节点"
    }

    function revealSelectedNode() {
        for (let i = 0; i < scene.treeNodes.length; ++i) {
            if (scene.treeNodes[i].id === scene.selectedId) {
                tree.positionViewAtIndex(i, ListView.Contain)
                return
            }
        }
    }
    function isNodeDescendantOf(targetId, ancestorId) {
        if (!targetId || !ancestorId)
            return false
        if (targetId === ancestorId)
            return true
        let curr = targetId
        while (curr !== 0) {
            let found = false
            for (let i = 0; i < scene.treeNodes.length; ++i) {
                const item = scene.treeNodes[i]
                if (item.id === curr) {
                    if (item.parentId === ancestorId)
                        return true
                    curr = item.parentId || 0
                    found = true
                    break
                }
            }
            if (!found || curr === 0)
                break
        }
        return false
    }

    function calculateTreeDrop(mouseXInTree, mouseYInTree) {
        const count = scene.treeNodes.length
        if (!count || !treeDragId) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = ""
            return
        }

        let index = tree.indexAt(mouseXInTree, mouseYInTree)
        if (index < 0) {
            if (mouseYInTree <= 0)
                index = 0
            else
                index = count - 1
        }
        index = Math.max(0, Math.min(count - 1, index))
        const target = scene.treeNodes[index]
        if (!target) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = ""
            return
        }

        if (target.id === treeDragId) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = "保持原位置"
            return
        }
        if (isNodeDescendantOf(target.id, treeDragId)) {
            treeDropTargetId = 0
            treeDropMode = 0
            treeDropHint = "无法移动至子孙节点"
            return
        }

        // Each delegate has height 36 + spacing 2 = 38
        const rowTop = index * 38 - tree.contentY
        const relY = mouseYInTree - rowTop

        let dragParentId = 0
        let dragIndex = -1
        for (let i = 0; i < count; ++i) {
            if (scene.treeNodes[i].id === treeDragId) {
                dragParentId = scene.treeNodes[i].parentId || 0
                dragIndex = i
                break
            }
        }

        if (target.id === scene.treeNodes[0].id) {
            treeDropTargetId = target.id
            treeDropMode = 2
            treeDropParentId = target.id
            treeDropBeforeId = 0
            treeDropHint = "作为 " + target.name + " 的子节点"
            return
        }

        if (relY < 10) {
            const targetParentId = target.parentId || 0
            if (targetParentId === dragParentId && dragIndex >= 0 && index === dragIndex + 1) {
                treeDropTargetId = 0
                treeDropMode = 0
                treeDropHint = "保持原位置"
                return
            }
            treeDropTargetId = target.id
            treeDropMode = 1
            treeDropParentId = targetParentId
            treeDropBeforeId = target.id
            treeDropHint = "插入到 " + target.name + " 之前"
        } else if (relY > 26) {
            const targetParentId = target.parentId || 0
            if (targetParentId === dragParentId && dragIndex >= 0 && index === dragIndex - 1) {
                treeDropTargetId = 0
                treeDropMode = 0
                treeDropHint = "保持原位置"
                return
            }
            treeDropTargetId = target.id
            treeDropMode = 3
            treeDropParentId = targetParentId
            const next = scene.treeNodes[index + 1]
            if (next && (next.parentId || 0) === targetParentId)
                treeDropBeforeId = next.id
            else
                treeDropBeforeId = 0
            treeDropHint = "插入到 " + target.name + " 之后"
        } else {
            treeDropTargetId = target.id
            treeDropMode = 2
            treeDropParentId = target.id
            treeDropBeforeId = 0
            treeDropHint = "作为 " + target.name + " 的子节点"
        }
    }
    function resetTreeDrag() {
        treeDragging = false
        treeDragId = 0
        treeDropTargetId = 0
        treeDropParentId = 0
        treeDropBeforeId = 0
        treeDropMode = 0
        treeDropHint = ""
        treeDragName = ""
    }
    Action {
        id: addGeometryAction
        text: "几何节点"
        shortcut: "Ctrl+Shift+G"
        onTriggered: scene.addGeometry()
    }
    Action {
        id: addTransformAction
        text: "变换节点"
        shortcut: "Ctrl+Shift+T"
        onTriggered: scene.addTransform()
    }
    Action {
        id: addBackdropAction
        text: "背景采样节点"
        onTriggered: scene.addBackdrop()
    }
    Action {
        text: "自定义渲染节点"
    }
    Action {
        id: addGroupAction
        text: "分组节点"
        onTriggered: scene.addBasic()
        shortcut: "Ctrl+Shift+N"
    }
    Action {
        id: moveUpAction
        text: "上移一层"
        enabled: !!win.sel.canLower
        onTriggered: scene.lowerSelected()
    }
    Action {
        id: moveDownAction
        text: "下移一层"
        enabled: !!win.sel.canRaise
        onTriggered: scene.raiseSelected()
    }
    Action {
        id: visibilityAction
        text: win.sel.visible ? "隐藏节点" : "显示节点"
        enabled: !!win.sel.id
        onTriggered: scene.setVisibleSelected(!win.sel.visible)
    }
    Action {
        id: dirtyAction
        text: "标记内容损伤"
        shortcut: "Ctrl+D"
        onTriggered: scene.markSelectedContentDirty()
    }
    Action {
        id: deleteAction
        text: "删除节点"
        shortcut: "Delete"
        onTriggered: scene.removeSelected()
    }

    Menu {
        id: addNodeMenu
        y: addNodeButton.height + 6
        MenuItem { action: addGeometryAction }
        MenuItem { action: addTransformAction }
        MenuItem { action: addBackdropAction }
        MenuSeparator {}
        MenuItem { action: addGroupAction }
    }

    Menu {
        id: nodeMenu
        title: win.sel.name || "节点"
        Menu {
            title: "新增子节点"
            MenuItem { action: addGeometryAction }
            MenuItem { action: addTransformAction }
            MenuItem { action: addBackdropAction }
            MenuItem { action: addGroupAction }
        }
        MenuItem { action: moveUpAction }
        MenuItem { action: moveDownAction }
        MenuItem { action: visibilityAction }
        MenuItem { action: dirtyAction }
        MenuSeparator {}
        MenuItem { action: deleteAction }
    }

    Menu {
        id: treeOptionsMenu
        y: treeOptionsButton.height + 6
        MenuItem {
            text: "选择根节点"
            onTriggered: scene.selectedId = scene.treeNodes.length ? scene.treeNodes[0].id : 0
        }
        MenuItem {
            text: "立即提交"
            onTriggered: scene.commit()
        }
        MenuSeparator {}
        MenuItem {
            text: "清空节点树"
            onTriggered: scene.clearTree()
    }
    }

    header: Rectangle {
        height: 72
        color: "#0f1625"
        border.color: win.borderColor
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 18
            spacing: 18

            ColumnLayout {
                spacing: 1
                Layout.minimumWidth: 250
                Label {
                    text: "损伤跟踪器"
                    color: win.textColor
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                }
                Label {
                    text: "场景树、遮挡剔除与多输出损伤"
                    color: win.mutedColor
                    font.pixelSize: 11
                }
            }

            Item { Layout.fillWidth: true }

            RowLayout {
                spacing: 10
                Label { text: "图例"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle {
                    width: 40; height: 16; radius: 4
                    border.color: "#8794ff"
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#a6b0ff" }
                        GradientStop { position: 1; color: "#4e5ee8" }
                    }
                }
                Label { text: "节点"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle { width: 40; height: 16; radius: 3; color: "#2dc470" }
                Label { text: "较旧损伤"; color: win.mutedColor; font.pixelSize: 10 }
                Rectangle { width: 40; height: 16; radius: 3; color: "#e83e4e" }
                Label { text: "最新损伤"; color: win.mutedColor; font.pixelSize: 10 }

                Rectangle { width: 1; height: 34; color: win.borderColor }

                ComboBox {
                    id: demoSceneCombo
                    Layout.preferredWidth: 150
                    model: scene.demoScenes
                    textRole: "text"
                    valueRole: "value"
                    onActivated: scene.loadDemoScene(currentValue)
                    ToolTip.visible: hovered
                    ToolTip.text: "选择只读自动演示场景"
                }
                Button {
                    text: scene.demoRunning ? "暂停演示" : "继续演示"
                    onClicked: scene.demoRunning = !scene.demoRunning
                }
                Label {
                    text: scene.demoRunning ? "自动演示中" : "演示已暂停"
                    color: scene.demoRunning ? "#8dc8e8" : "#ffcf70"
                    font.pixelSize: 10
                }
                Switch {
                    text: "自动提交"
                    checked: scene.autoCommit
                    onToggled: scene.autoCommit = checked
                    ToolTip.visible: hovered
                    ToolTip.text: "每次编辑后自动提交"
                }
                Button {
                    text: "提交"
                    highlighted: true
                    onClicked: scene.commit()
                }
            }
    }

        }
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Rectangle {
            SplitView.preferredWidth: 330
            SplitView.minimumWidth: 285
            color: win.panelColor
            border.color: win.borderColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: "节点树"
                            color: win.mutedColor
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: "共 " + scene.treeNodes.length + " 个节点"
                            color: win.textColor
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                        }
                    }
                    ToolButton {
                        id: treeOptionsButton
                        text: "•••"
                        onClicked: treeOptionsMenu.popup(treeOptionsButton, 0,
                                                         treeOptionsButton.height + 6)
                        ToolTip.visible: hovered
                        ToolTip.text: "节点树菜单"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        id: addNodeButton
                        Layout.fillWidth: true
                        text: "＋  新增子节点"
                        onClicked: addNodeMenu.popup(addNodeButton, 0,
                                                    addNodeButton.height + 6)
                    }
                    ToolButton {
                        text: "↑"
                        enabled: moveUpAction.enabled
                        onClicked: moveUpAction.trigger()
                        ToolTip.visible: hovered
                        ToolTip.text: "上移选中节点"
                    }
                    ToolButton {
                        text: "↓"
                        enabled: moveDownAction.enabled
                        onClicked: moveDownAction.trigger()
                        ToolTip.visible: hovered
                        ToolTip.text: "下移选中节点"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "新节点将添加到  " + (win.sel.name || "根节点") + "  下"
                    color: win.mutedColor
                    font.pixelSize: 10
                    elide: Text.ElideRight
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#0d1422"
                    radius: 8
                    border.color: win.borderColor

                    ListView {
                        id: tree
                        anchors.fill: parent
                        anchors.margins: 6
                        clip: true
                        model: scene.treeNodes
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds

                        delegate: Rectangle {
                            id: treeRow
                            required property var modelData
                            width: tree.width
                            height: 36
                            radius: 6
                            opacity: win.treeDragging && win.treeDragId === modelData.id ? 0.35 : 1.0
                            color: modelData.id === scene.selectedId
                                   ? "#283452" : (rowMouse.containsMouse ? "#182238" : "transparent")
                            border.color: modelData.id === scene.selectedId ? "#6476d9" : "transparent"

                            Rectangle {
                                visible: modelData.depth > 0
                                x: 12 + (modelData.depth - 1) * 17
                                width: 1
                                height: parent.height
                                color: "#2b3850"
                            }

                            // Indicator: insert before
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 1
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: -2
                                height: 4
                                radius: 2
                                color: "#ff8794"
                                z: 10
                            }
                            // Indicator: insert after
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 3
                                anchors.left: parent.left
                                anchors.right: parent.right
                                y: parent.height - 2
                                height: 4
                                radius: 2
                                color: "#ff8794"
                                z: 10
                            }
                            // Indicator: become child
                            Rectangle {
                                visible: win.treeDragging && win.treeDropTargetId === modelData.id
                                         && win.treeDropMode === 2
                                anchors.fill: parent
                                color: "#2d4e78"
                                border.color: "#6db8ff"
                                border.width: 2
                                radius: 6
                                z: 5
                            }

                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                leftPadding: 10 + modelData.depth * 17
                                spacing: 8
                                Rectangle {
                                    width: 18; height: 18; radius: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: modelData.type === "Geometry" ? "#596de8"
                                         : modelData.type === "Backdrop" ? "#0891b2"
                                         : modelData.type === "Transform" ? "#d28a2d" : "#556176"
                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData.type === "Geometry" ? "G"
                                            : modelData.type === "Backdrop" ? "B"
                                            : modelData.type === "Transform" ? "T" : "·"
                                        color: "#ffffff"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: -1
                                    Text {
                                        text: modelData.name || win.typeLabel(modelData.type)
                                        color: modelData.visible ? win.textColor : "#667289"
                                        font.pixelSize: 12
                                    }
                                    Text {
                                        text: win.typeLabel(modelData.type)
                                            + (modelData.occluded ? " · 已遮挡" : "")
                                            + (modelData.culled && !modelData.occluded ? " · 已剔除" : "")
                                            + (!modelData.visible ? " · 已隐藏" : "")
                                        color: win.mutedColor
                                        font.pixelSize: 9
                                    }
                                }
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton

                                onPressed: function(mouse) {
                                    scene.selectedId = modelData.id
                                    if (mouse.button === Qt.LeftButton) {
                                        beginUserInteraction()
                                        treePressX = mouse.x
                                        treePressY = mouse.y
                                        treeDragId = modelData.id
                                        treeDragName = modelData.name
                                        treeDragging = false
                                        var p = rowMouse.mapToItem(win.contentItem, mouse.x, mouse.y)
                                        treeGhostX = p.x + 10
                                        treeGhostY = p.y + 10
                                    } else if (mouse.button === Qt.RightButton) {
                                        nodeMenu.popup(treeRow, mouse.x, mouse.y)
                                    }
                                }
                                onPositionChanged: function(mouse) {
                                    if (!(mouse.buttons & Qt.LeftButton))
                                        return
                                    if (!win.treeDragging
                                            && (Math.abs(mouse.x - treePressX) + Math.abs(mouse.y - treePressY) > 4))
                                        win.treeDragging = true

                                    if (win.treeDragging) {
                                        var winPt = rowMouse.mapToItem(win.contentItem, mouse.x, mouse.y)
                                        treeGhostX = winPt.x + 12
                                        treeGhostY = winPt.y + 12
                                        var treePt = rowMouse.mapToItem(tree, mouse.x, mouse.y)
                                        calculateTreeDrop(treePt.x, treePt.y)
                                    }
                                }
                                onReleased: function(mouse) {
                                    if (mouse.button === Qt.LeftButton) {
                                        if (win.treeDragging) {
                                            const dragId = win.treeDragId
                                            const dropParentId = win.treeDropParentId
                                            const dropBeforeId = win.treeDropBeforeId
                                            const hasValidDrop = (dragId !== 0 && win.treeDropTargetId !== 0)
                                            win.resetTreeDrag()
                                            if (hasValidDrop) {
                                                Qt.callLater(function() {
                                                    scene.moveNode(dragId, dropParentId, dropBeforeId)
                                                })
                                            }
                                            win.resetTreeDrag()
                                            endUserInteraction()
                                            if (hasValidDrop) {
                                                Qt.callLater(function() {
                                                    scene.moveNode(dragId, dropParentId, dropBeforeId)
                                                })
                                            }
                                        } else {
                                            endUserInteraction()
                                        }
                                    }
                                }
                                onCanceled: {
                                    win.resetTreeDrag()
                                    endUserInteraction()
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    text: "拖动节点可调整顺序或改变父子关系；右键点击节点打开操作菜单。"
                    wrapMode: Text.Wrap
                    color: win.mutedColor
                    font.pixelSize: 10
                }
            }
        }

        Rectangle {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 480
            color: win.color

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        spacing: 0
                        Layout.fillWidth: true
                        Label {
                            text: "场景预览"
                            color: win.mutedColor
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: "拖动可见节点即可修改几何位置"
                            color: win.textColor
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                    }
                    Rectangle {
                        implicitWidth: damageStatus.implicitWidth + 18
                        implicitHeight: 28
                        radius: 14
                        color: (scene.damageRects.length || scene.damageRectsB.length) ? "#34202c" : "#172638"
                        border.color: (scene.damageRects.length || scene.damageRectsB.length) ? "#8e3f55" : "#2b526a"
                        Label {
                            id: damageStatus
                            anchors.centerIn: parent
                            text: (scene.damageRects.length || scene.damageRectsB.length)
                                  ? ("损伤  A " + scene.damageRects.length + "  ·  B " + scene.damageRectsB.length)
                                  : "无损伤"
                            color: (scene.damageRects.length || scene.damageRectsB.length) ? "#ffacba" : "#8dc8e8"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: "#090e18"
                    border.color: win.borderColor
                    clip: true

                    Item {
                        id: canvas
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true

                        Canvas {
                            anchors.fill: parent
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.fillStyle = "#0b1220"
                                ctx.fillRect(0, 0, width, height)
                                ctx.strokeStyle = "#172238"
                                ctx.lineWidth = 1
                                const s = 24
                                for (let x = 0.5; x < width; x += s) {
                                    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke()
                                }
                                for (let y = 0.5; y < height; y += s) {
                                    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke()
                                }
                            }
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                        }

                        // 视口 A 底层背景与点击选中区域 (位于图元下方)
                        Rectangle {
                            x: scene.viewportA.x; y: scene.viewportA.y
                            width: scene.viewportA.width; height: scene.viewportA.height
                            color: (scene.selectionType === 2 && scene.selectedViewportId === 0) ? "#147399ff" : "#04ffffff"
                            border.color: (scene.selectionType === 2 && scene.selectedViewportId === 0) ? "#7399ff" : "#78526079"
                            border.width: (scene.selectionType === 2 && scene.selectedViewportId === 0) ? 2.5 : 1.5
                            MouseArea {
                                anchors.fill: parent
                                onClicked: scene.selectViewport(0)
                            }
                        }

                        // 视口 B 底层背景与点击选中区域 (位于图元下方)
                        Rectangle {
                            x: scene.viewportB.x; y: scene.viewportB.y
                            width: scene.viewportB.width; height: scene.viewportB.height
                            color: (scene.selectionType === 2 && scene.selectedViewportId === 1) ? "#147399ff" : "#04ffffff"
                            border.color: (scene.selectionType === 2 && scene.selectedViewportId === 1) ? "#7399ff" : "#78526079"
                            border.width: (scene.selectionType === 2 && scene.selectedViewportId === 1) ? 2.5 : 1.5
                            MouseArea {
                                anchors.fill: parent
                                onClicked: scene.selectViewport(1)
                            }
                        }

                        Repeater {
                            model: scene.visualNodes
                            delegate: Item {
                                id: visualItem
                                
                                
                                width: 0
                                height: 0
                                visible: model.visible

                                transform: Matrix4x4 {
                                    matrix: Qt.matrix4x4(
                                        model.m11, model.m21, 0, model.dx,
                                        model.m12, model.m22, 0, model.dy,
                                        0,                        0,                        1, 0,
                                        model.m13, model.m23, 0, model.m33
                                    )
                                }

                                Rectangle {
                                    id: nodeRect
                                    property color baseColor: model.color
                                    property real fillAlpha: model.fullyOpaque
                                                             ? 1.0
                                                             : (model.isBackdrop ? 0.46 : 0.58)
                                    x: model.localX
                                    y: model.localY
                                    width: model.localWidth
                                    height: model.localHeight
                                    color: "transparent"
                                    radius: model.isBackdrop ? 12 : 7
                                    border.color: model.id === scene.selectedId
                                                  ? "#ffffff" : Qt.alpha(Qt.lighter(baseColor, 1.35), 0.95)
                                    border.width: model.id === scene.selectedId ? 2 : 1
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0
                                            color: Qt.alpha(Qt.lighter(nodeRect.baseColor, 1.35), nodeRect.fillAlpha)
                                        }
                                        GradientStop {
                                            position: 0.55
                                            color: Qt.alpha(nodeRect.baseColor, nodeRect.fillAlpha)
                                        }
                                        GradientStop {
                                            position: 1
                                            color: Qt.alpha(Qt.darker(nodeRect.baseColor, 1.45), nodeRect.fillAlpha)
                                        }
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        height: Math.min(30, parent.height)
                                        radius: parent.radius
                                        color: "#26000000"
                                    }
                                    Text {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 8
                                        text: model.name
                                            + (model.occluded ? "  ·  已遮挡" : (model.culled ? "  ·  已剔除" : ""))
                                        color: "#ffffff"
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                        style: Text.Outline
                                        styleColor: "#70000000"
                                    }
                                    MouseArea {
                                        property real lastCanvasX: 0
                                        property real lastCanvasY: 0
                                        property bool isDragging: false
                                        property bool interactionStarted: false

                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onPressed: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                scene.activateNode(model.id)
                                                isDragging = true
                                                interactionStarted = false
                                                var p = mapToItem(canvas, mouse.x, mouse.y)
                                                lastCanvasX = p.x
                                                lastCanvasY = p.y
                                            } else {
                                                scene.selectedId = model.id
                                                nodeMenu.popup(nodeRect, mouse.x, mouse.y)
                                            }
                                        }
                                        onPositionChanged: function(mouse) {
                                            if (!isDragging || !(mouse.buttons & Qt.LeftButton))
                                                return
                                            var p = mapToItem(canvas, mouse.x, mouse.y)
                                            var dx = p.x - lastCanvasX
                                            var dy = p.y - lastCanvasY
                                            if (dx === 0 && dy === 0)
                                                return
                                            if (!interactionStarted) {
                                                interactionStarted = true
                                                beginUserInteraction()
                                            }
                                            lastCanvasX = p.x
                                            lastCanvasY = p.y
                                            scene.moveSelectedBy(dx, dy)
                                        }
                                        onReleased: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                isDragging = false
                                                if (interactionStarted) {
                                                    interactionStarted = false
                                                    scene.finishSelectedMove()
                                                    endUserInteraction()
                                                }
                                            }
                                        }
                                        onCanceled: {
                                            isDragging = false
                                            if (interactionStarted) {
                                                interactionStarted = false
                                                scene.finishSelectedMove()
                                                endUserInteraction()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            x: scene.viewportA.x + 8; y: scene.viewportA.y + 6
                            width: labelVpA.implicitWidth + 12; height: 22; radius: 4
                            color: (scene.selectionType === 2 && scene.selectedViewportId === 0) ? "#3355aa" : "#203048"
                            Label {
                                id: labelVpA
                                anchors.centerIn: parent
                                text: "输出 A (Primary)"
                                color: (scene.selectionType === 2 && scene.selectedViewportId === 0) ? "#ffffff" : win.mutedColor
                                font.pixelSize: 9; font.weight: Font.DemiBold
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: scene.selectViewport(0)
                            }
                        }
                        Rectangle {
                            x: scene.viewportB.x + 8; y: scene.viewportB.y + 6
                            width: labelVpB.implicitWidth + 12; height: 22; radius: 4
                            color: (scene.selectionType === 2 && scene.selectedViewportId === 1) ? "#3355aa" : "#203048"
                            Label {
                                id: labelVpB
                                anchors.centerIn: parent
                                text: "输出 B (Secondary)"
                                color: (scene.selectionType === 2 && scene.selectedViewportId === 1) ? "#ffffff" : win.mutedColor
                                font.pixelSize: 9; font.weight: Font.DemiBold
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: scene.selectViewport(1)
                            }
                        }

                        DamageOverlay {
                            anchors.fill: parent
                            frames: scene.damageFrames
                            historyDuration: win.damageHistoryDuration
                            refreshRate: win.sceneRefreshRate
                        }

                        Connections {
                            target: scene
                            function onSelectedIdChanged() { Qt.callLater(win.revealSelectedNode) }
                        }
                    }
                }
            }
        }

        Rectangle {
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 270
            color: win.panelColor
            border.color: win.borderColor

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Label {
                    text: "属性检查器"
                    color: win.mutedColor
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "损伤残留时间"
                        color: win.textColor
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        id: damageDurationEditor
                        Layout.preferredWidth: 112
                        from: 100
                        to: 10000
                        stepSize: 100
                        value: win.damageHistoryDuration
                        editable: true
                        onValueModified: win.damageHistoryDuration = value
                        ToolTip.visible: hovered
                        ToolTip.text: "损伤帧保持可见的时间"
                    }
                    Label {
                        text: "ms"
                        color: win.mutedColor
                        font.pixelSize: 10
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "刷新率"
                        color: win.textColor
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        id: refreshRateEditor
                        Layout.preferredWidth: 112
                        from: 1
                        to: 360
                        stepSize: 1
                        value: win.sceneRefreshRate
                        editable: true
                        onValueModified: win.sceneRefreshRate = value
                        ToolTip.visible: hovered
                        ToolTip.text: "拖动提交和损伤重绘的刷新率"
                    }
                    Label {
                        text: "Hz"
                        color: win.mutedColor
                        font.pixelSize: 10
                    }
                    ToolButton {
                        text: "↺"
                        onClicked: win.sceneRefreshRate = win.detectedRefreshRate
                        ToolTip.visible: hovered
                        ToolTip.text: "恢复为当前屏幕刷新率"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 62
                    radius: 8
                    color: win.raisedColor
                    border.color: win.borderColor
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Rectangle {
                            width: 34; height: 34; radius: 9
                            color: scene.selectionType === 2 ? "#0284c7"
                                 : win.sel.type === "Geometry" ? "#596de8"
                                 : win.sel.type === "Backdrop" ? "#0891b2"
                                 : win.sel.type === "Transform" ? "#d28a2d" : "#556176"
                            Label {
                                anchors.centerIn: parent
                                text: scene.selectionType === 2 ? "V"
                                    : win.sel.type ? win.sel.type.charAt(0) : "–"
                                color: "#ffffff"
                                font.bold: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                text: scene.selectionType === 2 ? (scene.selectedViewportProps.name || "视口")
                                    : (win.sel.name || "未选择对象")
                                color: win.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: scene.selectionType === 2 ? ("输出视口  ·  ID " + scene.selectedViewportProps.id)
                                    : win.sel.type ? (win.typeLabel(win.sel.type) + "  ·  ID " + win.sel.id)
                                                   : "在节点树或画布中选择节点/视口"
                                color: win.mutedColor
                                font.pixelSize: 10
                            }
                        }
                    }
                }
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth

                    // Viewport 属性编辑器 (当选中 Viewport 时展示)
                    ColumnLayout {
                        width: parent.width
                        visible: scene.selectionType === 2
                        spacing: 10

                        Label { text: "视口基础属性"; color: win.accentColor; font.pixelSize: 10; font.bold: true }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: "视口 X"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 4000
                                value: scene.selectedViewportProps.x || 0; editable: true
                                onValueModified: scene.setViewportRect(value, scene.selectedViewportProps.y || 0, scene.selectedViewportProps.width || 360, scene.selectedViewportProps.height || 480)
                            }

                            Label { text: "视口 Y"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 4000
                                value: scene.selectedViewportProps.y || 0; editable: true
                                onValueModified: scene.setViewportRect(scene.selectedViewportProps.x || 0, value, scene.selectedViewportProps.width || 360, scene.selectedViewportProps.height || 480)
                            }

                            Label { text: "宽度 W"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 50; to: 4000
                                value: scene.selectedViewportProps.width || 360; editable: true
                                onValueModified: scene.setViewportRect(scene.selectedViewportProps.x || 0, scene.selectedViewportProps.y || 0, value, scene.selectedViewportProps.height || 480)
                            }

                            Label { text: "高度 H"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 50; to: 4000
                                value: scene.selectedViewportProps.height || 480; editable: true
                                onValueModified: scene.setViewportRect(scene.selectedViewportProps.x || 0, scene.selectedViewportProps.y || 0, scene.selectedViewportProps.width || 360, value)
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }

                        Label { text: "输出投影矩阵"; color: win.accentColor; font.pixelSize: 10; font.bold: true }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: "缩放比例"; color: win.mutedColor; font.pixelSize: 10 }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["1.0x (原比例)", "1.25x (高清缩放)", "1.5x (缩放)", "2.0x (4K 缩放)", "0.75x (缩小)"]
                                currentIndex: scene.selectedViewportProps.scale === 1.25 ? 1
                                            : scene.selectedViewportProps.scale === 1.5 ? 2
                                            : scene.selectedViewportProps.scale === 2.0 ? 3
                                            : scene.selectedViewportProps.scale === 0.75 ? 4 : 0
                                onActivated: {
                                    var s = (currentIndex === 1 ? 1.25 : (currentIndex === 2 ? 1.5 : (currentIndex === 3 ? 2.0 : (currentIndex === 4 ? 0.75 : 1.0))))
                                    scene.setViewportScale(s)
                                }
                            }

                            Label { text: "旋转角度"; color: win.mutedColor; font.pixelSize: 10 }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["0° (正常)", "90° (垂直屏幕)", "180° (倒置)", "270° (逆向垂直)"]
                                currentIndex: scene.selectedViewportProps.rotation === 90 ? 1
                                            : scene.selectedViewportProps.rotation === 180 ? 2
                                            : scene.selectedViewportProps.rotation === 270 ? 3 : 0
                                onActivated: {
                                    var deg = (currentIndex === 1 ? 90 : (currentIndex === 2 ? 180 : (currentIndex === 3 ? 270 : 0)))
                                    scene.setViewportRotation(deg)
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }

                        Label { text: "Swapchain 多缓冲与 Buffer Age"; color: win.accentColor; font.pixelSize: 10; font.bold: true }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "Buffer 数量"; color: win.mutedColor; font.pixelSize: 10 }
                            ComboBox {
                                Layout.fillWidth: true
                                model: ["2 (双缓冲 Double Buffering)", "3 (三缓冲 Triple Buffering)", "4 (四缓冲)"]
                                currentIndex: (scene.selectedViewportProps.bufferCount === 3) ? 1 : ((scene.selectedViewportProps.bufferCount === 4) ? 2 : 0)
                                onActivated: scene.setViewportBufferCount(currentIndex + 2)
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "持续注入 Buffer 损伤"; color: win.textColor; font.pixelSize: 10; Layout.fillWidth: true }
                            Switch {
                                checked: !!scene.selectedViewportProps.swapchainEnabled
                                onToggled: scene.setViewportSwapchainEnabled(checked)
                                ToolTip.visible: hovered
                                ToolTip.text: "模拟双缓冲/三缓冲 Buffer 切换时补偿的历史 Damage"
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: "相对 X"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: scene.selectedViewportProps.damageX || 30; editable: true
                                onValueModified: scene.setViewportSwapchainDamageRect(value, scene.selectedViewportProps.damageY || 40, scene.selectedViewportProps.damageW || 180, scene.selectedViewportProps.damageH || 140)
                            }

                            Label { text: "相对 Y"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: scene.selectedViewportProps.damageY || 40; editable: true
                                onValueModified: scene.setViewportSwapchainDamageRect(scene.selectedViewportProps.damageX || 30, value, scene.selectedViewportProps.damageW || 180, scene.selectedViewportProps.damageH || 140)
                            }

                            Label { text: "宽度 W"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 1; to: 2000
                                value: scene.selectedViewportProps.damageW || 180; editable: true
                                onValueModified: scene.setViewportSwapchainDamageRect(scene.selectedViewportProps.damageX || 30, scene.selectedViewportProps.damageY || 40, value, scene.selectedViewportProps.damageH || 140)
                            }

                            Label { text: "高度 H"; color: win.mutedColor; font.pixelSize: 10 }
                            SpinBox {
                                Layout.fillWidth: true; from: 1; to: 2000
                                value: scene.selectedViewportProps.damageH || 140; editable: true
                                onValueModified: scene.setViewportSwapchainDamageRect(scene.selectedViewportProps.damageX || 30, scene.selectedViewportProps.damageY || 40, scene.selectedViewportProps.damageW || 180, value)
                            }
                        }
                    }

                    // Node 属性编辑器 (当选中 Node 时展示)
                    ColumnLayout {
                        width: parent.width
                        visible: scene.selectionType === 1

                        Label { text: "常规"; color: win.mutedColor; font.pixelSize: 9; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "固定名称"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true
                                text: win.sel.name || ""
                                color: win.textColor
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "可见"; color: win.textColor; Layout.fillWidth: true }
                            Switch { checked: !!win.sel.visible; onToggled: scene.setVisibleSelected(checked) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "包含自身内容 (hasContent)"; color: win.textColor; Layout.fillWidth: true }
                            Switch {
                                checked: win.sel.hasContent !== false
                                onToggled: scene.setHasContentSelected(checked)
                                ToolTip.visible: hovered
                                ToolTip.text: "关闭后节点变为纯几何容器，自身不参与绘制与遮挡，但子节点正常显示"
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            visible: !!win.sel.isGeometry
                            Label { text: "完全不透明"; color: win.textColor; Layout.fillWidth: true }
                            Switch { checked: !!win.sel.fullyOpaque; onToggled: scene.setFullyOpaqueSelected(checked) }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }
                        Label {
                            visible: !!win.sel.isGeometry
                            text: "几何属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isGeometry
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "X"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.x); editable: true
                                onValueModified: scene.setRectSelected(value, win.num(win.sel.y), win.num(win.sel.w), win.num(win.sel.h))
                            }
                            Label { text: "Y"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.y); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), value, win.num(win.sel.w), win.num(win.sel.h))
                            }
                            Label { text: "宽度"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: win.num(win.sel.w); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), win.num(win.sel.y), value, win.num(win.sel.h))
                            }
                            Label { text: "高度"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 2000
                                value: win.num(win.sel.h); editable: true
                                onValueModified: scene.setRectSelected(win.num(win.sel.x), win.num(win.sel.y), win.num(win.sel.w), value)
                            }
                        }

                        Label {
                            visible: !!win.sel.isTransform
                            text: "变换属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isTransform
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "水平位移"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.tx); editable: true
                                onValueModified: scene.setTranslationSelected(value, win.num(win.sel.ty))
                            }
                            Label { text: "垂直位移"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: -2000; to: 2000
                                value: win.num(win.sel.ty); editable: true
                                onValueModified: scene.setTranslationSelected(win.num(win.sel.tx), value)
                            }
                            Label { text: "旋转轴"; color: win.mutedColor }
                            ComboBox {
                                id: rotationAxisCombo
                                Layout.fillWidth: true
                                model: ["X 轴", "Y 轴", "Z 轴"]
                                currentIndex: win.num(win.sel.rotationAxis)
                                onActivated: scene.setRotationSelected(rotationAngleEditor.value, currentIndex)
                            }
                            Label { text: "旋转角度"; color: win.mutedColor }
                            SpinBox {
                                id: rotationAngleEditor
                                Layout.fillWidth: true; from: -360; to: 360
                                value: win.num(win.sel.rotation); editable: true
                                onValueModified: scene.setRotationSelected(value, rotationAxisCombo.currentIndex)
                            }
                            Label { text: "缩放 X"; color: win.mutedColor }
                            TextField {
                                Layout.fillWidth: true
                                text: win.num(win.sel.scaleX).toFixed(2)
                                onEditingFinished: scene.setScaleSelected(parseFloat(text) || 1, win.num(win.sel.scaleY))
                            }
                            Label { text: "缩放 Y"; color: win.mutedColor }
                            TextField {
                                Layout.fillWidth: true
                                text: win.num(win.sel.scaleY).toFixed(2)
                                onEditingFinished: scene.setScaleSelected(win.num(win.sel.scaleX), parseFloat(text) || 1)
                            }
                        }
                        Label {
                            visible: !!win.sel.isTransform
                            text: "矩阵"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isTransform
                            Layout.fillWidth: true
                            columns: 3
                            columnSpacing: 8
                            rowSpacing: 4
                            Label { text: "m11"; color: win.mutedColor }
                            Label { text: win.num(win.sel.m11).toFixed(3); color: win.textColor }
                            Label { text: "m12  " + win.num(win.sel.m12).toFixed(3); color: win.textColor }
                            Label { text: "m21"; color: win.mutedColor }
                            Label { text: win.num(win.sel.m21).toFixed(3); color: win.textColor }
                            Label { text: "m22  " + win.num(win.sel.m22).toFixed(3); color: win.textColor }
                            Label { text: "dx"; color: win.mutedColor }
                            Label { text: win.num(win.sel.dx).toFixed(3); color: win.textColor }
                            Label { text: "dy  " + win.num(win.sel.dy).toFixed(3); color: win.textColor }
                        }
                        Label {
                            visible: !!win.sel.isBackdrop
                            text: "背景采样属性"
                            color: win.mutedColor
                            font.pixelSize: 9
                            font.bold: true
                        }
                        GridLayout {
                            visible: !!win.sel.isBackdrop
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Label { text: "扩张范围"; color: win.mutedColor }
                            SpinBox {
                                Layout.fillWidth: true; from: 0; to: 80
                                value: win.num(win.sel.expansion); editable: true
                                onValueModified: scene.setExpansionSelected(value)
                            }
                            Label { text: "裁剪扩张"; color: win.mutedColor }
                            Switch {
                                checked: win.sel.clipExpansion !== false
                                onToggled: scene.setClipExpansionSelected(checked)
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: win.borderColor }
                        Label {
                            text: "运行时状态（只读）"
                            color: win.mutedColor; font.pixelSize: 9; font.bold: true
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "可见区域"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                text: {
                                    var r = win.sel.visibleRegion
                                    if (!r) return "—"
                                    if (r.width === 0 || r.height === 0) return "空（被遮挡）"
                                    return "(" + r.x + "," + r.y + ") " + r.width + "x" + r.height
                                }
                                color: win.sel.visibleRegion && win.sel.visibleRegion.width === 0 ? "#e57373" : "#66bb6a"
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "不透明区域"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                text: {
                                    var r = win.sel.opaqueRegion
                                    if (!r) return "—"
                                    if (r.width === 0 || r.height === 0) return "空"
                                    return "(" + r.x + "," + r.y + ") " + r.width + "x" + r.height
                                }
                                color: win.textColor
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "世界包围盒"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                text: {
                                    var r = win.sel.worldBounds
                                    if (!r) return "—"
                                    return "(" + r.x + "," + r.y + ") " + r.width + "x" + r.height
                                }
                                color: win.textColor
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "子树包围盒"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                text: {
                                    var r = win.sel.subtreeBounds
                                    if (!r) return "—"
                                    return "(" + r.x + "," + r.y + ") " + r.width + "x" + r.height
                                }
                                color: win.textColor
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "脏标记"; color: win.mutedColor }
                            Label {
                                Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                text: win.sel.dirty !== undefined ? (win.sel.dirty ? "脏" : "干净") : "—"
                                color: win.sel.dirty ? "#ff9800" : win.textColor
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Button { text: "上移"; enabled: moveUpAction.enabled; onClicked: moveUpAction.trigger() }
                            Button { text: "下移"; enabled: moveDownAction.enabled; onClicked: moveDownAction.trigger() }
                            Item { Layout.fillWidth: true }
                            Button { text: "标记损伤"; enabled: dirtyAction.enabled; onClicked: dirtyAction.trigger() }
                        }
                        Button {
                            Layout.fillWidth: true
                            text: "删除选中节点"
                            enabled: deleteAction.enabled
                            onClicked: deleteAction.trigger()
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: treeDragGhost
        parent: win.contentItem
        z: 99999
        visible: win.treeDragging
        x: win.treeGhostX
        y: win.treeGhostY
        width: 230
        height: 38
        radius: 8
        color: "#f01c2a3f"
        border.color: "#7399ff"
        border.width: 1.5

        Row {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8
            Rectangle {
                width: 20
                height: 20
                radius: 5
                anchors.verticalCenter: parent.verticalCenter
                color: "#4e73df"
                Label {
                    anchors.centerIn: parent
                    text: "↕"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: -1
                Label {
                    text: win.treeDragName || "节点"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
                Label {
                    text: win.treeDropHint || "拖动中..."
                    color: "#8cd3ff"
                    font.pixelSize: 9
                }
            }
        }
    }
}
