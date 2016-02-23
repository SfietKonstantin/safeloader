import QtQuick 2.0
import sl 1.0

Item {
    id: container
    anchors.fill: parent

    Grid {
        anchors.fill: parent
        columns: 2
        rows: 2

        Repeater {
            model: 4
            SafeLoader {
                width: container.width / 2
                height: container.height / 2
                source: "qrc:/test.qml"
            }
        }
    }
}
