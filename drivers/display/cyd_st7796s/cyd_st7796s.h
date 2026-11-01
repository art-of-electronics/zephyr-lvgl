#ifndef CYD_ST7796S_H
#define CYD_ST7796S_H

/**
 * @brief Configure the CYD ST7796S display panel.
 * @return 0 on success, negative errno on failure.
 * 
 * @note Applies panel-specific settings through the MIPI DBI interface.
 */
int cyd_st7796s_configure(void);

#endif // CYD_ST7796S_H
