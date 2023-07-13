global-incdirs-y += .

srcs-$(PLATFORM_FLAVOR_sama5d2_xplained) += main.c
srcs-$(PLATFORM_FLAVOR_sama5d27_som1_ek) += main.c
srcs-$(PLATFORM_FLAVOR_sama7g54_ek) += platform_sama7g54.c
srcs-y += sam_sfr.c freq.c
srcs-$(CFG_AT91_MATRIX) += matrix.c
srcs-$(CFG_PL310) += sam_pl310.c
srcs-$(CFG_SCMI_MSG_DRIVERS) += scmi_server.c

subdirs-y += pm
subdirs-y += drivers
subdirs-y += nsec-service
