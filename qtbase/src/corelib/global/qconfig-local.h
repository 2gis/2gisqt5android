/* Dialogs */
#ifndef QT_NO_ERRORMESSAGE
#  define QT_NO_ERRORMESSAGE
#endif
#ifndef QT_NO_INPUTDIALOG
#  define QT_NO_INPUTDIALOG
#endif
#ifndef QT_NO_PRINTDIALOG
#  define QT_NO_PRINTDIALOG
#endif
#ifndef QT_NO_PRINTPREVIEWDIALOG
#  define QT_NO_PRINTPREVIEWDIALOG
#endif
#ifndef QT_NO_PROGRESSDIALOG
#  define QT_NO_PROGRESSDIALOG
#endif
#ifndef QT_NO_WIZARD
#  define QT_NO_WIZARD
#endif

/* File I/O */
#if !defined QT_NO_PROCESS && defined Q_OS_ANDROID
#  define QT_NO_PROCESS
#endif

/* Images */
#ifndef QT_NO_IMAGEFORMAT_BMP
#  define QT_NO_IMAGEFORMAT_BMP
#endif
#ifndef QT_NO_IMAGEFORMAT_PPM
#  define QT_NO_IMAGEFORMAT_PPM
#endif
#ifndef QT_NO_IMAGEFORMAT_XBM
#  define QT_NO_IMAGEFORMAT_XBM
#endif
#ifndef QT_NO_MOVIE
#  define QT_NO_MOVIE
#endif

/* Internationalization */
#ifndef QT_NO_BIG_CODECS
#  define QT_NO_BIG_CODECS
#endif
#ifndef QT_NO_CODECS
#  define QT_NO_CODECS
#endif

/* Kernel */
#ifndef QT_NO_EFFECTS
#  define QT_NO_EFFECTS
#endif
#ifndef QT_NO_SHAREDMEMORY
#  define QT_NO_SHAREDMEMORY
#endif

/* Networking */
#ifndef QT_NO_SOCKS5
#  define QT_NO_SOCKS5
#endif
#ifndef QT_NO_FTP
#  define QT_NO_FTP
#endif

/* Painting */
#ifndef QT_NO_PAINT_DEBUG
#  define QT_NO_PAINT_DEBUG
#endif
#ifndef QT_NO_PRINTER
#  define QT_NO_PRINTER
#endif
/* Styles */
#ifndef QT_NO_STYLE_WINDOWSCE
#  define QT_NO_STYLE_WINDOWSCE
#endif
#ifndef QT_NO_STYLE_WINDOWSMOBILE
#  define QT_NO_STYLE_WINDOWSMOBILE
#endif
#ifndef QT_NO_STYLE_WINDOWSVISTA
#  define QT_NO_STYLE_WINDOWSVISTA
#endif
#ifndef QT_NO_STYLE_WINDOWSXP
#  define QT_NO_STYLE_WINDOWSXP
#endif
