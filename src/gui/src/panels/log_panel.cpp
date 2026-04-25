#include "gui/panels/log_panel.hpp"

#include "core/logging.hpp"
#include "layout/dock_utils.hpp"

#include <imgui.h>

namespace ggm::gui {

void
drawLogPanel(ImGuiID dockspaceId) noexcept
{
  prepareDockedWindow("Log", dockspaceId);
  ImGui::Begin("Log");

  auto sink = logging::guiSink();
  auto messages = sink ? sink->last_formatted() : std::vector<std::string>{};

  ImGui::Text("Messages: %zu", messages.size());
  ImGui::Separator();

  ImGui::BeginChild(
    "##LogScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

  for (const auto& msg : messages) {
    ImGui::TextUnformatted(msg.c_str());
  }

  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0F);
  }

  ImGui::EndChild();
  ImGui::End();
}

}
