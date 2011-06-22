
#ifndef BCM_SDHC26_SPI_H_
#define BCM_SDHC26_SPI_H_

// Function prototypes - non optimized functions
static int spi_init ( void );
static int spi_freq_max ( int );
static void spi_freq_wait ( cycles_t );
static void spi_cs_ass ( void );
static void spi_cs_dea ( void );
static unsigned char spi_io ( unsigned char );
static unsigned char spi_card_cmd_r ( unsigned char, unsigned int, unsigned char, unsigned char *, int );
static unsigned char spi_card_read_blk ( unsigned char, unsigned int, unsigned char, unsigned char *, int );

// Function prototypes - optimized functions
static inline void spi_cs_ass_o ( void );
static inline void spi_cs_dea_o ( void );
static inline void spi_io_ff_v_o ( void );
static inline void spi_io_2ff_v_o ( void );
static inline void spi_io_6ff_v_o ( void );
static inline unsigned char spi_io_ff_o ( void );
static inline void spi_io_v_o ( unsigned char );
static inline unsigned char spi_card_write_multi_o ( unsigned int, unsigned char *, int, unsigned char, unsigned char );
static inline unsigned char spi_card_read_multi_o ( unsigned int, unsigned char *, int, unsigned char, unsigned char );

#endif /* BCM_SDHC26_SPI_H_ */
