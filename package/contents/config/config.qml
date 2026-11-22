// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT

import QtQuick
import org.kde.plasma.configuration

ConfigModel {
    ConfigCategory {
        name: i18n("常规")
        icon: "preferences-system"
        source: "config/ConfigGeneral.qml"
    }
}
