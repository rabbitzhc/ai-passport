idf_component_register(
    SRCS "main.c"
         "axe_logic.c"
         "axe_ui.c"
         "../assets/fonts/axe_king_font_16.c"
         "../assets/fonts/axe_king_font_24.c"
         "../assets/images/axe_king_bead_64.c"
         "../assets/images/axe_king_bead_96.c"
         "../assets/images/axe_king_hands_96.c"
         "../assets/images/axe_king_cry_96.c"
         "../assets/images/axe_king_cute_96.c"
         "../assets/images/axe_king_shades_96.c"
         "../assets/images/axe_king_teeth_96.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support
)
