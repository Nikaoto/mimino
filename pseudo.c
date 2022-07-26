

states:
1. reading
2. writing_headers  // writes headers only
3. writing_body     // writes res->buf or res->file contents
4. writing_finished // closes or recycles the connection
5. closed

reading -> reading // on partial
reading -> writing_headers // on success
reading -> closed // on failure (or timeout, which is handled outside)

writing_headers -> writing_headers // on partial
writing_headers -> writing_body // on success (GET request)
writing_headers -> writing_finished // on success (HEAD request)
writing_headers -> closed // on failure or timeout or client closed

writing_body -> writing_body // on partial
writing_body -> writing_finished // on success
writing_body -> closed // on failure or timeout or client closed

case CONN_STATE_READING:
  // .. bla bla
  conn->state = CONN_STATE_WRITING_HEADERS;
  break;

case CONN_STATE_WRITING_HEADERS:
  int status = write_headers(&serv, conn);
  switch (status) {
  case W_PARTIAL_WRITE:
    continue;
    break;

  case W_CLIENT_CLOSED:
  case W_FATAL_ERROR:
    // print error stuff
    conn->state = CONN_STATE_CLOSING;
    do_conn_state(&serv, conn);
    break;

  case W_COMPLETE_WRITE:
    if (!strcmp(conn->req->method, "HEAD")) {
      conn->state = CONN_STATE_WRITING_FINISHED;
      do_conn_state(&serv, conn);
    } else {
      conn->state = CONN_STATE_WRITING_BODY;
      do_conn_state(&serv, conn);
    }
    break;
  }

case CONN_STATE_WRITING_BODY:
  int status = write_body(&serv, conn);
  switch (status) {
  case W_PARTIAL_WRITE:
    continue;
    break;

  case W_CLIENT_CLOSED:
  case W_FATAL_ERROR:
    // print error stuff
    conn->state = CONN_STATE_CLOSING;
    do_conn_state(&serv, conn);
    break;

  case W_COMPLETE_WRITE:
    conn->state = CONN_STATE_WRITING_FINISHED;
    do_conn_state(&serv, conn);
    break;
  }
  break;

case CONN_STATE_WRITING_FINISHED:
  if (conn->keep_alive) {
    recycle_connection(&serv, fd_i);
    conn->state = CONN_STATE_READING;
  } else {
    conn->state = CONN_STATE_CLOSING;
    do_conn_state(&serv, conn);
  }
  break;

case CONN_STATE_CLOSING:
  free_connection_parts(conn);
  close_connection(&serv, fd_i);
  break;

