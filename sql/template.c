/*
 * Database communication template as an example for SQL Hybserv client.
 * -kre
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <libpq-fe.h>

void fatal(PGconn *conn)
{
  PQfinish(conn);
  exit(EXIT_FAILURE);
}

int main(void)
{
  int n, i, j;
  PGconn     *conn;
  PGresult   *res;
  FILE *debug;

  /* Connect to database */
  conn = PQconnectdb("dbname=foo");
  if (PQstatus(conn) == CONNECTION_BAD)
  {
    fprintf(stderr, "Connection to database failed.\n");
    fprintf(stderr, "%s", PQerrorMessage(conn));
    fatal(conn);
  }

  /* Start SQL client/backend trace */
  debug = fopen("/tmp/trace.out","w");
  PQtrace(conn, debug);

  /* Begin transactions */
  res = PQexec(conn, "BEGIN");
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
  {
      fprintf(stderr, "BEGIN command failed\n");
      PQclear(res);
      fatal(conn);
  }

  /* Declare cursor for select */
  res = PQexec(conn, "DECLARE mycursor CURSOR FOR SELECT * FROM foo");
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "DECLARE CURSOR command failed\n");
    PQclear(res);
    fatal(conn);
  }
  PQclear(res);

  /* Fetch all records from cursor */
  res = PQexec(conn, "FETCH ALL in mycursor");
  if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "FETCH ALL command didn't return tuples properly\n");
    PQclear(res);
    fatal(conn);
  }

  /* Print all fetched names of records and data */
  n = PQnfields(res);

  for (i = 0; i < n; ++i)
    printf("%-15s", PQfname(res, i));

  printf("\n----------------------------------\n");

  for (i = 0; i < PQntuples(res); i++)
  {
      for (j = 0; j < n; j++)
          printf("%-15s", PQgetvalue(res, i, j));
      printf("\n");
  }

  PQclear(res);

  /* Commit transactions */
  res = PQexec(conn, "COMMIT");
  PQclear(res);

  /* Finish and close trace */
  PQfinish(conn);
  fclose(debug);

  return 0;
}

