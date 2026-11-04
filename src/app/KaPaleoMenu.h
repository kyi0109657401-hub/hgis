#pragma once

#include <functional>

class QToolButton;
class QWidget;

namespace KaPaleoMenu {
void install(QToolButton* button, QWidget* host, const std::function<void()>& trigger);
}  // namespace KaPaleoMenu
