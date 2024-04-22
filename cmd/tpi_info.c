// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 Sven Rademakers <sven@turingpi.com>
 *
 */

#include <stdio.h>

#include <bloblist.h>
#include <board_info.h>
#include <command.h>
#include <common.h>
#include <env.h>

// Returns the semver version pointed to by `version_ptr` as a char*, prefixed
// with 'v'. e.g. v2.5.1
//
// The version field is encoded inside an uint16_t.
// they are grouped as followed:
// 5 bits for the major version
// 5 bits for the minor version
// 6 bits for the patch version
//
// therefore the maximum supported version is 32.32.64
//
// The passed buffer should be at least 10 bytes long:
// 'vXX.XX.XX/0' = 10
void parse_version_field(char *buffer, uint16_t version) {
  sprintf(buffer, "v%d.%d.%d", version >> 11, (version >> 6) & 0x1F,
          version & 0x3F);
}

void try_split_identifier(char *buffer, int pos) {
  char *found = buffer;
  for (int i = 0; i < pos; ++i) {
    ++found;
    found = strchr(found, '.');
    if (!found) {
      return;
    }
  }

  if (found && found != buffer) {
    *found = '\0';
  }
}

#define ENV_NAME(member) "tpi_" #member
#define SAVE_U16_OR_RETURN(member, env_name)                                   \
  if (sprintfu16(info, env_name, offsetof(tpi_board_info, member))) {          \
    return CMD_RET_FAILURE;                                                    \
  }
#define SAVE_U32_OR_RETURN(member, env_name)                                   \
  if (sprintfu32(info, env_name, offsetof(tpi_board_info, member))) {          \
    return CMD_RET_FAILURE;                                                    \
  }
#define SAVE_STR_OR_RETURN(member, env_name)                                   \
  if (sprintfstr(info, env_name, offsetof(tpi_board_info, member))) {          \
    return CMD_RET_FAILURE;                                                    \
  }
#define SAVE_MAC_OR_RETURN(env_name)                                           \
  if (sprintfmac(info, env_name)) {                                            \
    return CMD_RET_FAILURE;                                                    \
  }

int save_to_env(const char *env_name, const char *buffer) {
  if (env_set(env_name, buffer)) {
    printf("Failed to set environment variable '%s'\n", env_name);
    return CMD_RET_FAILURE;
  }

  return CMD_RET_SUCCESS;
}

int sprintfu32(tpi_board_info *info, const char *env_name, size_t offsetoff) {
  char buffer[9] = {0};
  sprintf(buffer, "%08x", *(uint32_t *)((void *)info + offsetoff));
  return save_to_env(env_name, buffer);
}

int sprintfu16(tpi_board_info *info, const char *env_name, size_t offsetoff) {
  char buffer[5] = {0};
  sprintf(buffer, "%04x", *(uint16_t *)((void *)info + offsetoff));
  return save_to_env(env_name, buffer);
}

int sprintfstr(tpi_board_info *info, const char *env_name, size_t offsetoff) {
  char buffer[32] = {0};
  snprintf(buffer, sizeof(buffer), "%s", ((char *)info) + offsetoff);
  return save_to_env(env_name, buffer);
}

int sprintfmac(tpi_board_info *info, const char *env_name) {
  char buffer[13] = {0};
  sprintf(buffer,"%02X%02X%02X%02X%02X%02X\n", info->mac[0], info->mac[1], info->mac[2],
         info->mac[3], info->mac[4], info->mac[5]);
  return save_to_env(env_name, buffer);
}

int handle_version(tpi_board_info *info, char *env_name, const char option) {
  char buffer[10] = {0};
  parse_version_field(buffer, info->hw_version);
  try_split_identifier(buffer, option - '0');

  if (env_set(env_name, buffer) != 0) {
    printf("Failed to set environment variable '%s'\n", env_name);
    return CMD_RET_FAILURE;
  }

  return CMD_RET_SUCCESS;
}

int handle_all(tpi_board_info *info) {
  if (handle_version(info, ENV_NAME(hw_version), '3')) {
    return CMD_RET_FAILURE;
  }

  SAVE_U16_OR_RETURN(hdr_version, ENV_NAME(hdr_version));
  SAVE_U32_OR_RETURN(crc32, ENV_NAME(crc32));
  SAVE_U16_OR_RETURN(factory_date, ENV_NAME(factory_date));
  SAVE_STR_OR_RETURN(factory_serial, ENV_NAME(factory_serial));
  SAVE_STR_OR_RETURN(product_name, ENV_NAME(product_name));
  SAVE_MAC_OR_RETURN(ENV_NAME(mac));
  return CMD_RET_SUCCESS;
}

// This function reads back the turing-pi board info that is set by a customized
// SPL. it expects a `BLOBLISTT_U_BOOT_SPL_HANDOFF` tag to be present inside the
// bloblist that contains a correct encoded tpi_board_info struct.
int do_tpi_info(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]) {
  char *name = "all";
  if (argc >= 2) {
    name = argv[1];
  }

  char *env_name = NULL;
  if (argc >= 3) {
    env_name = argv[2];
    if (env_name[0] == '\0') {
      printf("provided an empty var name");
      return CMD_RET_FAILURE;
    }
  }

  int init_res = bloblist_maybe_init();
  if (init_res != 0) {
    printf("Error initializing bloblist: %d", init_res);
    return CMD_RET_FAILURE;
  }

  // Get the blob data
  tpi_board_info *info =
      (tpi_board_info *)bloblist_find(BLOBLISTT_U_BOOT_SPL_HANDOFF, 0);

  if (!info) {
    puts("Error: spl handoff blob not found\n");
    return CMD_RET_FAILURE;
  }

  if (compute_crc(info) != info->crc32) {
    printf("Error: tpi_board_info: invalid CRC\n");
  } else {
    env_set(ENV_NAME(crc_ok), "y");
  }

  if (!strcmp("all", name)) {
    return handle_all(info);
  } else if (!strcmp("hw_version", name)) {
    char option = '3';
    if (argc >= 4) {
      option = *argv[3];
    }
    return handle_version(info, env_name, option);
  } else if (!strcmp("reserved", name)) {
    SAVE_U16_OR_RETURN(reserved, env_name);
  } else if (!strcmp("hdr_version", name)) {
    SAVE_U16_OR_RETURN(hdr_version, env_name);
  } else if (!strcmp("crc32", name)) {
    SAVE_U32_OR_RETURN(crc32, env_name);
  } else if (!strcmp("factory_date", name)) {
    SAVE_U16_OR_RETURN(factory_date, env_name);
  } else if (!strcmp("mac", name)) {
    SAVE_MAC_OR_RETURN(env_name);
  } else if (!strcmp("factory_serial", name)) {
    SAVE_STR_OR_RETURN(factory_serial, env_name);
  } else if (!strcmp("product_name", name)) {
    SAVE_STR_OR_RETURN(product_name, env_name);
  } else {
    printf("Error: %s is not an member\n", name);
    return CMD_RET_FAILURE;
  }
  return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
    tpi_info, 4, 0, do_tpi_info, "retrieves the turing-pi board-info from SPL",
    "Usage:\n"
    "    `tpi_info <member> <env_name> <optional args>`\n\n"
    "    <member>:\n"
    "     - hdr_version\n"
    "     - crc32\n"
    "     - hw_version\n"
    "     - factory_date // days since May 1st 2024\n"
    "     - product_name\n"
    "     - mac\n"
    "     - factory_serial\n\n"
    "    <env_name>: name of env to store the version in\n"
    "Specialized commands:\n\n"
    "    `tpi_info`\n"
    "        // loads all available data in environment using 'tpi_' prefix\n"
    "    `tpi_info version <env_name> <identifier len>`\n"
    "        <identifier len>: 3= full, 2=major+minor, "
    "1=major\n");
