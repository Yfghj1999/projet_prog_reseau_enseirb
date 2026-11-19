Adresse  = IP + PORT sinon on parle d'autre chose

      SERVEUR                         CLIENT
      socket()                        socket()
        │                               │
      bind()     ← associe IP:port      │
        │                               │
      listen()   ← file d'attente       │
        │                               │
      accept() ◄──── 3-way handshake ── connect()
        │  (retourne un NOUVEAU fd)      │
      recv()/send() ◄──── données ────► send()/recv()
        │                               │
      close()                         close()


// struct in_addr{
//     unint32_t s_addr;
// }
// struct sockaddr_in{
//     short sin_family;
//     u_short sin_port;
//     struct in_addr sin_addr;
//     char sin_zero[8];
// }
