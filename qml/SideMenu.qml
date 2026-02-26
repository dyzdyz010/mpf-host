import QtQuick
import QtQuick.Controls
import QtQuick.Layouts


/**
 * Side menu / navigation drawer
 * Displays plugin-registered menu items
 */
Rectangle {
    id: root

    property bool expanded: true
    property int collapsedWidth: 60
    property int expandedWidth: 240
    property string currentRoute: ""

    signal itemClicked(string id, string route)

    implicitWidth: expanded ? expandedWidth : collapsedWidth
    color: Theme ? Theme.surfaceColor : "#F5F5F5"

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme ? Theme.primaryColor : "#2196F3"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                // Logo
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: "white"

                    Text {
                        anchors.centerIn: parent
                        text: "⚡"
                        font.pixelSize: 18
                    }
                }

                // Title (hidden when collapsed)
                Label {
                    text: "MPF"
                    font.pixelSize: 18
                    font.bold: true
                    color: "white"
                    visible: root.expanded
                    Layout.fillWidth: true
                }

                // Collapse button
                ToolButton {
                    text: root.expanded ? "◀" : "▶"
                    onClicked: root.expanded = !root.expanded

                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    background: Rectangle {
                        color: parent.hovered ? Qt.rgba(1, 1, 1,
                                                        0.2) : "transparent"
                        radius: 4
                    }
                }
            }
        }

        // Menu items
        ListView {
            id: menuList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8

            clip: true
            spacing: 4

            model: AppMenu ? AppMenu.items : []

            delegate: MenuItemCustom {
                width: menuList.width
                expanded: root.expanded

                itemId: modelData.id || ""
                title: modelData.label || ""
                icon: modelData.icon || ""
                route: modelData.route || ""
                badge: modelData.badge || ""
                enabled: modelData.enabled !== false
                selected: modelData.route === root.currentRoute

                onClicked: {
                    root.itemClicked(modelData.id, modelData.route)
                }
            }

            // Empty state
            Label {
                anchors.centerIn: parent
                visible: menuList.count === 0
                text: root.expanded ? qsTr("No plugins loaded") : "..."
                color: Theme ? Theme.textSecondaryColor : "#757575"
                font.pixelSize: 13
            }
        }

        // Footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: Qt.darker(Theme ? Theme.surfaceColor : "#F5F5F5", 1.05)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                // Theme toggle
                ToolButton {
                    text: Theme && Theme.isDark ? "☀️" : "🌙"
                    onClicked: {
                        if (Theme) {
                            Theme.setTheme(Theme.isDark ? "Light" : "Dark")
                        }
                    }

                    ToolTip.visible: hovered && root.expanded
                    ToolTip.text: qsTr("Toggle theme")
                }

                Item {
                    Layout.fillWidth: true
                    visible: root.expanded
                }

                // Version
                Label {
                    text: "v" + (App ? App.version : "1.0")
                    font.pixelSize: 11
                    color: Theme ? Theme.textSecondaryColor : "#757575"
                    visible: root.expanded
                }
            }
        }
    }
}
