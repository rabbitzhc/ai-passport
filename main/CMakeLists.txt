idf_component_register(
    SRCS "main.c"
         "axe_logic.c"
         "axe_ui.c"
         "../assets/fonts/axe_king_font_16.c"
         "../assets/fonts/axe_king_font_24.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support
)
