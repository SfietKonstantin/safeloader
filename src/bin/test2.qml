import QtQuick 2.0

Rectangle {
    anchors.fill: parent
    color: "red"

    Image {
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
        source: "fosdem.jpg"
        fillMode: Image.PreserveAspectFit

        NumberAnimation on rotation {
            from: 0; to: 360; duration: 3000; loops: Animation.Infinite
        }
        SequentialAnimation on scale{
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.5; to: 1; duration: 1500
            }
            NumberAnimation {
                from: 1; to: 0.5; duration: 1500
            }
        }
    }
}
