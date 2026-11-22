// SPDX-License-Identifier: GPL-2.0-or-later
// A selectable panel preset; it does not replace or mutate the user's panel.
const panel = new Panel;
const geometry = screenGeometry(panel.screen);

panel.location = "bottom";
panel.alignment = "center";
panel.length = geometry.width;
panel.height = 2 * Math.ceil(gridUnit * 2.5 / 2);

const launcher = panel.addWidget("org.kde.plasma.kickoff");
launcher.currentConfigGroup = ["General"];
launcher.writeConfig("icon", "better-on");
launcher.writeConfig("menuLabel", "Better On");

panel.addWidget("org.kde.plasma.icontasks");
panel.addWidget("org.kde.plasma.marginsseparator");
panel.addWidget("org.kde.plasma.systemtray");
panel.addWidget("org.kde.plasma.digitalclock");
panel.addWidget("org.kde.plasma.showdesktop");
