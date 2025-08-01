
OUT_DIR = build/

OBJECT_FILES =\
	$(OUT_DIR)startup.o \
	$(OUT_DIR)services/ai.o \
	$(OUT_DIR)controllers/account.o \
	$(OUT_DIR)controllers/message.o

SITE_NAME = driima
OUTPUT_FILE = $(OUT_DIR)$(SITE_NAME).a

LIBAPP = ../libapp/
LIBWEB = ../libweb/
LIBAPP_A = $(LIBAPP)build/libapp.a
LIBWEB_A = $(LIBWEB)build/libweb.a

INCLUDE_FILES = ./*.h $(LIBAPP)src/*.h $(LIBWEB)src/*.h \
	models/*.h controllers/*.h

#-------------------------------------------------

# compiler flags
BASIC_FLAGS = -std=c99 -g -Wall -Wextra -Wconversion -Wwrite-strings -pedantic
WARN_TO_ERROR = -Werror=implicit-function-declaration -Werror=implicit-int -Wincompatible-pointer-types
SECURITY_FLAGS = -Wformat -Werror=format-security -fstack-protector-strong
ADVANCED_FLAGS = -fPIC -fvisibility=hidden
DEFINITIONS = '-D__LIB__="$(SITE_NAME)"' -D_REENTRANT
APACHE_DIRS ?= -I /usr/include/apache2 -I /usr/include/apr-1.0
INCLUDES_DIRS = -I ~/.local/lib $(APACHE_DIRS) -I $(LIBAPP)src -I $(LIBWEB)src
CC_FLAGS = $(BASIC_FLAGS) $(WARN_TO_ERROR) $(SECURITY_FLAGS) \
	$(ADVANCED_FLAGS) $(DEFINITIONS) $(INCLUDES_DIRS) $(CFLAGS)

# archiver flags
AR_FLAGS = -crs

#-------------------------------------------------

VALID = $(OUT_DIR)validate.done
default: $(VALID)

$(OUT_DIR):
	mkdir -p $(OUT_DIR)models/
	mkdir -p $(OUT_DIR)services/
	mkdir -p $(OUT_DIR)controllers/

# compile .c files to .o files
$(OUT_DIR)%.o: ./%.c $(INCLUDE_FILES) | $(OUT_DIR)
	$(CC) $(CC_FLAGS) -c -o $@ $<

# archive .o files to .a file
$(OUTPUT_FILE): $(OBJECT_FILES)
	$(AR) $(AR_FLAGS) $(OUTPUT_FILE) $(OBJECT_FILES)

# remove all created files
clean:
	$(RM) -r $(OUT_DIR)

#-------------------------------------------------

SO_FILE = $(OUT_DIR)mod_$(SITE_NAME).so

$(SO_FILE): $(OUTPUT_FILE) $(LIBWEB_A) $(LIBAPP_A) module.c
	$(CC) --shared -fPIC $(APACHE_DIRS) -DSITE_NAME=$(SITE_NAME) module.c $(OUTPUT_FILE) $(LIBWEB_A) $(LIBAPP_A) -lmysqlclient -lcrypto -lcurl -o $(SO_FILE)

# Check against an unresolved symbol
$(VALID): $(SO_FILE)
	nm -D $(SO_FILE) | grep " U " | grep -vE "@|ap_|apr_" && exit 1 || exit 0
	touch $(VALID)

PUB_FILE = $(OUT_DIR)publish.tar.gz
publish: $(PUB_FILE)

$(PUB_FILE): $(VALID)
	tar -czhf $(PUB_FILE) $(SO_FILE) --exclude=.git public views migrations ai i18n .htaccess settings.json
	scp $(PUB_FILE) vps:

start: $(VALID)
	apachectl -X -k start

restart: $(VALID)
	apachectl -k restart

#-------------------------------------------------
