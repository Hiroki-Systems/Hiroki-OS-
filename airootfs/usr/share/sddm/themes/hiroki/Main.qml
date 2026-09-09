import QtQuick 2.0
import SddmComponents 2.0

Rectangle {
    width: 1920; height: 1080
    color: "#0D0D1A"
    Image {
        source: "file:///usr/share/hiroki/wallpapers/sakura-gradient.jpg"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        opacity: 0.9
    }
    Column {
        anchors.centerIn: parent
        spacing: 20
        Image { source: "hiroki-logo.png"; width: 260; height: 80; fillMode: Image.PreserveAspectFit; anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: "Hiroki OS 1.0 Sakura"; color: "#EAEAEA"; font.pixelSize: 22; font.bold: true; anchors.horizontalCenter: parent.horizontalCenter }
        TextField { id: name; width: 260; placeholderText: "Kullanıcı adı"; anchors.horizontalCenter: parent.horizontalCenter }
        PasswordField { id: password; width: 260; placeholderText: "Parola"; anchors.horizontalCenter: parent.horizontalCenter }
        Button { text: "Giriş Yap"; width: 260; anchors.horizontalCenter: parent.horizontalCenter; onClicked: sddm.login(name.text, password.text, session.index) }
        ComboBox { id: session; width: 260; model: sessionModel; index: sessionModel.lastIndex; anchors.horizontalCenter: parent.horizontalCenter }
    }
    Text { text: "hiroki-os.org"; color: "#A0A0B8"; font.pixelSize: 11; anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 20 }
}
