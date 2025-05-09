#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_VERBOSE

#ifdef CONFIG_VERBOSE
#define LOG(msg) printf("Log: %s\n", msg)
#else
#define LOG(msg) do {} while (0)
#endif

// Uncomment the following line to enable the feature
// #define FEATURE_ENABLED

#endif // CONFIG_H