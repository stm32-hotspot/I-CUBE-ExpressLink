#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 0
  #define PRINTF(...)    printf(__VA_ARGS__)
#else
  #define PRINTF(...)
#endif /* defined(USE_TRACE) && USE_TRACE != 0 */


#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 0
  #define PRINTF_ERROR(...)    printf(__VA_ARGS__)
#else
  #define PRINTF_ERROR(...)
#endif /* defined(USE_TRACE) && USE_TRACE != 0 */


#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 1
  #define PRINTF_INFO(...)    printf(__VA_ARGS__)
#else
  #define PRINTF_INFO(...)
#endif /* defined(USE_TRACE) && USE_TRACE != 0 */


#if defined(DEBUG_LEVEL) && DEBUG_LEVEL > 2
  #define PRINTF_DEBUG(...)    printf(__VA_ARGS__)
#else
  #define PRINTF_DEBUG(...)
#endif /* defined(USE_TRACE) && USE_TRACE != 0 */
