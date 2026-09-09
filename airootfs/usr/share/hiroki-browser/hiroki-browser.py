#!/usr/bin/env python3
# Hiroki Browser - Hafif WebView Tarayıcı
# PyQt6 QWebEngineView tabanlı

import sys
from PyQt6.QtCore import QUrl, Qt
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QToolBar, QLineEdit,
    QPushButton, QStatusBar, QTabWidget, QWidget, QVBoxLayout
)
from PyQt6.QtWebEngineWidgets import QWebEngineView
from PyQt6.QtWebEngineCore import QWebEngineProfile, QWebEnginePage


class BrowserTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.web_view = QWebEngineView()
        layout.addWidget(self.web_view)

        self.web_view.setUrl(QUrl("https://www.google.com"))

    def navigate(self, url):
        if not url.startswith(("http://", "https://")):
            url = "https://" + url
        self.web_view.setUrl(QUrl(url))

    def url(self):
        return self.web_view.url()

    def back(self):
        self.web_view.back()

    def forward(self):
        self.web_view.forward()

    def reload(self):
        self.web_view.reload()


class HirokiBrowser(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Hiroki Browser")
        self.setMinimumSize(1024, 600)

        self.tabs = QTabWidget()
        self.tabs.setTabsClosable(True)
        self.tabs.tabCloseRequested.connect(self.close_tab)
        self.tabs.currentChanged.connect(self.update_url_bar)
        self.setCentralWidget(self.tabs)

        self.create_toolbar()
        self.add_new_tab(QUrl("https://www.google.com"), "Yeni Sayfa")
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)

    def create_toolbar(self):
        toolbar = QToolBar("Gezinme")
        toolbar.setMovable(False)
        self.addToolBar(toolbar)

        self.back_btn = QPushButton("<")
        self.back_btn.setFixedWidth(30)
        self.back_btn.clicked.connect(self.navigate_back)
        toolbar.addWidget(self.back_btn)

        self.forward_btn = QPushButton(">")
        self.forward_btn.setFixedWidth(30)
        self.forward_btn.clicked.connect(self.navigate_forward)
        toolbar.addWidget(self.forward_btn)

        self.reload_btn = QPushButton("\u21bb")
        self.reload_btn.setFixedWidth(30)
        self.reload_btn.clicked.connect(self.reload_page)
        toolbar.addWidget(self.reload_btn)

        self.url_bar = QLineEdit()
        self.url_bar.returnPressed.connect(self.navigate_to_url)
        toolbar.addWidget(self.url_bar)

        self.go_btn = QPushButton("Git")
        self.go_btn.setFixedWidth(50)
        self.go_btn.clicked.connect(self.navigate_to_url)
        toolbar.addWidget(self.go_btn)

        self.new_tab_btn = QPushButton("+")
        self.new_tab_btn.setFixedWidth(30)
        self.new_tab_btn.clicked.connect(lambda: self.add_new_tab(QUrl("https://www.google.com"), "Yeni Sayfa"))
        toolbar.addWidget(self.new_tab_btn)

    def add_new_tab(self, url, title="Yeni Sayfa"):
        tab = BrowserTab()
        tab.navigate(url.toString())
        index = self.tabs.addTab(tab, title)
        self.tabs.setCurrentIndex(index)

        tab.web_view.titleChanged.connect(lambda t, idx=index: self.tabs.setTabText(idx, t))
        tab.web_view.urlChanged.connect(lambda u, idx=index: self.tabs.setTabText(idx, u.toString().split("/")[2] if "/" in u.toString() else u.toString()))

    def close_tab(self, index):
        if self.tabs.count() > 1:
            self.tabs.removeTab(index)
        else:
            self.close()

    def current_tab(self):
        return self.tabs.currentWidget()

    def navigate_to_url(self):
        url = self.url_bar.text()
        tab = self.current_tab()
        if tab:
            tab.navigate(url)

    def navigate_back(self):
        tab = self.current_tab()
        if tab:
            tab.back()

    def navigate_forward(self):
        tab = self.current_tab()
        if tab:
            tab.forward()

    def reload_page(self):
        tab = self.current_tab()
        if tab:
            tab.reload()

    def update_url_bar(self, index):
        tab = self.tabs.widget(index)
        if tab and hasattr(tab, 'url'):
            url = tab.url()
            self.url_bar.setText(url.toString())
            self.url_bar.setCursorPosition(0)


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("Hiroki Browser")
    app.setOrganizationName("Hiroki OS")

    profile = QWebEngineProfile.defaultProfile()
    profile.setHttpUserAgent(
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) HirokiBrowser/1.0 Chrome/131.0 Safari/537.36"
    )

    browser = HirokiBrowser()
    browser.show()

    if len(sys.argv) > 1:
        url = sys.argv[1]
        if not url.startswith(("http://", "https://")):
            url = "https://" + url
        browser.add_new_tab(QUrl(url), "Sayfa")

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
