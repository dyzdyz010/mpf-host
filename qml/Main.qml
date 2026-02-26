import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: qsTr("Qt Modular Plugin Framework")

    // Theme bindings
    color: Theme ? Theme.backgroundColor : "#FFFFFF"

    // Track current route for menu highlighting
    property string currentRoute: ""

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Side menu
        SideMenu {
            id: sideMenu
            Layout.fillHeight: true
            currentRoute: root.currentRoute

            onItemClicked: function (id, route) {
                // Get the page URL from Navigation service
                if (Navigation && route) {
                    var pageUrl = Navigation.getPageUrl(route)
                    if (pageUrl) {
                        contentLoader.loadPage(pageUrl)
                        root.currentRoute = route
                    }
                }
            }
        }

        // Separator
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: Theme ? Qt.darker(Theme.surfaceColor, 1.1) : "#E0E0E0"
        }

        // Main content area
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Header bar
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: Theme ? Theme.surfaceColor : "#F5F5F5"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12

                    // Home button (replaces back button)
                    ToolButton {
                        visible: root.currentRoute !== ""
                        text: "🏠"
                        font.pixelSize: 20
                        onClicked: {
                            contentLoader.goHome()
                            root.currentRoute = ""
                        }

                        background: Rectangle {
                            color: parent.hovered ? Qt.alpha(
                                                        Theme ? Theme.textColor : "#212121",
                                                        0.1) : "transparent"
                            radius: 4
                        }
                    }

                    // Page title
                    Label {
                        text: contentLoader.item ? (contentLoader.item.pageTitle
                                                           || contentLoader.item.title
                                                           || "Home") : "Home"
                        font.pixelSize: 20
                        font.weight: Font.Medium
                        color: Theme ? Theme.textColor : "#212121"
                        Layout.fillWidth: true
                    }

                    // Plugin count badge
                    Rectangle {
                        visible: AppMenu && AppMenu.count > 0
                        implicitWidth: countLabel.implicitWidth + 16
                        implicitHeight: 24
                        radius: 12
                        color: Theme ? Qt.alpha(Theme.primaryColor,
                                                0.15) : "#E3F2FD"

                        Label {
                            id: countLabel
                            anchors.centerIn: parent
                            text: (AppMenu ? AppMenu.count : 0) + " plugins"
                            font.pixelSize: 12
                            color: Theme ? Theme.primaryColor : "#2196F3"
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme ? Qt.darker(Theme.surfaceColor, 1.1) : "#E0E0E0"
            }

            // Content area - simple Loader (no StackView)
            Loader {
                id: contentLoader
                objectName: "contentLoader"
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                // Default to welcome page
                sourceComponent: WelcomePage {}
                
                // Load plugin page by URL
                function loadPage(url) {
                    if (url && url.toString().length > 0) {
                        source = url
                    } else {
                        sourceComponent = WelcomePage
                    }
                }
                
                // Go back to welcome page
                function goHome() {
                    source = ""
                    sourceComponent = WelcomePage
                }
                
                onStatusChanged: {
                    if (status === Loader.Error) {
                        console.error("Failed to load page:", source)
                    }
                }
            }
        }
    }

    // Welcome page component
    component WelcomePage: Page {
        property string pageTitle: qsTr("Home")
        property string route: ""

        background: Rectangle {
            color: Theme ? Theme.backgroundColor : "#FFFFFF"
        }

        ScrollView {
            id: welcomeScroll
            anchors.fill: parent
            anchors.margins: 24

            ColumnLayout {
                width: welcomeScroll.availableWidth
                spacing: 24

                // Hero section
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    radius: Theme ? Theme.radiusLarge : 16
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Theme ? Theme.primaryColor : "#2196F3"
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.darker(
                                       Theme ? Theme.primaryColor : "#2196F3",
                                       1.3)
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            text: "⚡"
                            font.pixelSize: 48
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Label {
                            text: "Qt Modular Plugin Framework"
                            font.pixelSize: 28
                            font.bold: true
                            color: "white"
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Label {
                            text: "Version " + (App ? App.version : "1.0.0")
                            font.pixelSize: 14
                            color: Qt.rgba(1, 1, 1, 0.8)
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

                // Stats cards
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16

                    StatCard {
                        Layout.fillWidth: true
                        title: qsTr("Loaded Plugins")
                        value: AppMenu ? AppMenu.count : 0
                        icon: "🔌"
                        accentColor: "#4CAF50"
                    }

                    StatCard {
                        Layout.fillWidth: true
                        title: qsTr("Services")
                        value: "5"
                        icon: "⚙️"
                        accentColor: "#2196F3"
                    }

                    StatCard {
                        Layout.fillWidth: true
                        title: qsTr("Theme")
                        value: Theme ? Theme.name : "Light"
                        icon: Theme && Theme.isDark ? "🌙" : "☀️"
                        accentColor: "#FF9800"
                    }
                }

                // Getting started
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Getting Started")

                    background: Rectangle {
                        color: Theme ? Theme.surfaceColor : "#F5F5F5"
                        radius: Theme ? Theme.radiusMedium : 8
                        y: parent.topPadding - 12
                        height: parent.height - parent.topPadding + 12
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 12

                        Label {
                            text: AppMenu
                                  && AppMenu.count > 0 ? qsTr("Select a plugin from the sidebar to get started.") : qsTr("No plugins are currently loaded. Add plugins to the plugins/ directory and restart.")
                            font.pixelSize: 14
                            color: Theme ? Theme.textSecondaryColor : "#757575"
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        // Quick links when plugins are loaded
                        Flow {
                            visible: AppMenu && AppMenu.count > 0
                            Layout.fillWidth: true
                            spacing: 8

                            Repeater {
                                model: AppMenu ? AppMenu.items : []

                                Button {
                                    text: (modelData.icon
                                           || "") + " " + (modelData.label
                                                           || "")
                                    onClicked: {
                                        if (Navigation && modelData.route) {
                                            var pageUrl = Navigation.getPageUrl(modelData.route)
                                            if (pageUrl) {
                                                contentLoader.loadPage(pageUrl)
                                                root.currentRoute = modelData.route
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Core services info
                GroupBox {
                    Layout.fillWidth: true
                    title: qsTr("Core Services")

                    background: Rectangle {
                        color: Theme ? Theme.surfaceColor : "#F5F5F5"
                        radius: Theme ? Theme.radiusMedium : 8
                        y: parent.topPadding - 12
                        height: parent.height - parent.topPadding + 12
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 24
                        rowSpacing: 8

                        ServiceLabel {
                            name: "Navigation"
                            status: Navigation ? "✓" : "✗"
                        }
                        ServiceLabel {
                            name: "Settings"
                            status: Settings ? "✓" : "✗"
                        }
                        ServiceLabel {
                            name: "Theme"
                            status: Theme ? "✓" : "✗"
                        }
                        ServiceLabel {
                            name: "Menu"
                            status: AppMenu ? "✓" : "✗"
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    // Stat card component
    component StatCard: Rectangle {
        id: statCard
        property string title: ""
        property var value: ""
        property string icon: ""
        property color accentColor: Theme ? Theme.primaryColor : "#2196F3"

        implicitHeight: 100
        radius: Theme ? Theme.radiusMedium : 8
        color: Theme ? Theme.surfaceColor : "#F5F5F5"

        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            Rectangle {
                width: 48
                height: 48
                radius: 24
                color: Qt.alpha(statCard.accentColor, 0.15)

                Text {
                    anchors.centerIn: parent
                    text: statCard.icon
                    font.pixelSize: 24
                }
            }

            ColumnLayout {
                spacing: 4

                Label {
                    text: statCard.title
                    font.pixelSize: 13
                    color: Theme ? Theme.textSecondaryColor : "#757575"
                }

                Label {
                    text: String(statCard.value)
                    font.pixelSize: 24
                    font.bold: true
                    color: statCard.accentColor
                }
            }

            Item {
                Layout.fillWidth: true
            }
        }
    }

    // Service label component
    component ServiceLabel: RowLayout {
        property string name: ""
        property string status: "?"

        spacing: 8

        Label {
            text: status
            font.pixelSize: 14
            color: status === "✓" ? (Theme ? Theme.successColor : "#4CAF50") : (Theme ? Theme.errorColor : "#F44336")
        }

        Label {
            text: name
            font.pixelSize: 14
            color: Theme ? Theme.textColor : "#212121"
        }
    }

    // Error dialog
    Loader {
        id: errorDialogLoader
        active: false
        sourceComponent: ErrorDialog {}
    }

}
