/**
 * @file History.h
 * @brief Terminal UI page that displays the full conversation history.
 *
 * @details Reads all entries from conversation.json (written by
 * ConversationLogger) and renders them as a scrollable, paginated
 * list in the terminal. The user can navigate back to the main menu
 * or close the window entirely.
 * @author Meridith Shang
 */
#pragma once

/**
 * @author Meridith Shang
 * @brief Launches the interactive conversation history page.
 *
 * @details Reads from conversation.json and displays all logged
 * command/response exchanges in reverse-chronological or sequential order.
 * Blocks until the user chooses to navigate away.
 *
 * @return true if the user navigated back to the main menu;
 *         false if the user closed the window or an unrecoverable
 *         read error occurred.
 */
bool runHistoryPage();