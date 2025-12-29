#ifndef ROUTE_H
#define ROUTE_H

#include <mysql/mysql.h>
#include "../service/auth_service.h"

void route_request(ClientSession *client, char *json_str, MYSQL *db_conn);

#endif