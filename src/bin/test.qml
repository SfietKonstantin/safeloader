import QtQuick 2.0
import sl 1.0

Item {
    id: container
    anchors.fill: parent

    Rectangle {
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
        color: "yellow"

        NumberAnimation on rotation {
            from: 0; to: 360; duration: 4000; loops: Animation.Infinite
        }
        SequentialAnimation on scale{
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.5; to: 1; duration: 2000
            }
            NumberAnimation {
                from: 1; to: 0.5; duration: 2000
            }
        }

        Grid {
            anchors.fill: parent
            columns: 2
            rows: 2

            Repeater {
                model: 4
                SafeLoader {
                    width: container.width / 2
                    height: container.height / 2
                    source: "qrc:/test2.qml"
                }
            }
        }
    }
}
