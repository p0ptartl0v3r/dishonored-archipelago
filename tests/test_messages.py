from dishonored.client.messages import PROGRESSION, TRAP, USEFUL, colored, item_color, received_line, sent_line


def test_item_colours_follow_archipelago():
    assert item_color(PROGRESSION) == "AF99EF"
    assert item_color(USEFUL) == "6D8BE8"
    assert item_color(TRAP) == "FA8072"
    assert item_color(0) == "00EEEE"


def test_received_line_is_coloured_segments_with_spaces_kept():
    assert received_line("Progressive Blink", PROGRESSION, "Alice", False, "Pub: River Rune") == (
        "{FFFFFF}Received {AF99EF}Progressive Blink{FFFFFF} from {FAFAD2}Alice{FFFFFF} ("
        "{00FF7F}Pub: River Rune{FFFFFF})"
    )
    assert received_line("Whalebone", 0, "me", True, "Pub: Shop Rune") == (
        "{FFFFFF}Found {00EEEE}Whalebone{FFFFFF} ({00FF7F}Pub: Shop Rune{FFFFFF})"
    )


def test_sent_line():
    assert sent_line("Hookshot", USEFUL, "Bob", "Prologue: Get Framed") == (
        "{FFFFFF}Sent {6D8BE8}Hookshot{FFFFFF} to {FAFAD2}Bob{FFFFFF} ({00FF7F}Prologue: Get Framed{FFFFFF})"
    )


def test_braces_and_newlines_cannot_break_a_line():
    assert colored("weird {name}\nhere", "FFFFFF") == "{FFFFFF}weird (name} here"


def test_chat_line_names_the_speaker():
    from dishonored.client.messages import chat_line

    assert chat_line("Alice", False, "gg") == "{FAFAD2}Alice{FFFFFF}: gg"
    assert chat_line("me", True, "hi") == "{EE00EE}me{FFFFFF}: hi"


def test_main_menu_notice_names_the_missing_key():
    from dishonored.client.messages import sent_to_menu_notice

    notice = sent_to_menu_notice("Kennels Zone Key")
    assert notice.startswith("{FA8072}Sent back to the main menu: you don't have the ")
    assert "{AF99EF}Kennels Zone Key{FA8072} yet" in notice
