#include "shortcuttext.h"

namespace ShortcutText {

QString primaryModifier()
{
#ifdef Q_OS_MAC
    return QStringLiteral("Command");
#else
    return QStringLiteral("Ctrl");
#endif
}

QString alternateModifier()
{
#ifdef Q_OS_MAC
    return QStringLiteral("Option");
#else
    return QStringLiteral("Alt");
#endif
}

QString panTooltip()
{
    return QStringLiteral(
        "Pan view\n"
        "Hold Space for temporary pan from any interaction mode.");
}

QString zoomTooltip()
{
    return QStringLiteral(
        "Zoom view\n"
        "Hold Z for temporary zoom from any interaction mode.");
}

QString editPointsTooltip()
{
    return QStringLiteral(
        "Edit the active point layer.\n"
        "Click empty space to add, drag a point to move it, and %1+click to delete.")
        .arg(alternateModifier());
}

QString interactionShortcutsMarkdown()
{
    const QString primary = primaryModifier();
    const QString alternate = alternateModifier();
    return QStringLiteral(
        "## Navigation\n\n"
        "- **Pan:** hold `Space` from any interaction mode.\n"
        "- **Zoom:** hold `Z` from any interaction mode, then drag with the left mouse button.\n"
        "- **Mouse wheel:** zoom under the cursor.\n\n"
        "## Point Selection\n\n"
        "- **Toggle point:** `%1+click` a point.\n"
        "- **Box selection:** `%1+drag` on empty image space.\n"
        "- **Add box selection:** `%1+Shift+drag` on empty image space.\n\n"
        "## Point Editing\n\n"
        "- **Add point:** click empty image space while editing a point layer.\n"
        "- **Move point:** drag a point while editing its layer.\n"
        "- **Delete point:** `%2+click` a point while editing. If the point is part of a multiple selection, all selected points are deleted.\n\n"
        "## Split Views\n\n"
        "- **Split viewer:** split the current image viewer into two panels.\n"
        "- **Sync split views:** synchronize pan between split viewers.\n"
        "- **Sync zoom:** synchronize zoom between split viewers when sync is enabled.\n")
        .arg(primary, alternate);
}

}
