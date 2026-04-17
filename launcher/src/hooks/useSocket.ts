import { useEffect, useRef, useState } from "react";
import { io, Socket } from "socket.io-client";

const DEFAULT_URL = import.meta.env.VITE_MATCHMAKING_URL ?? "http://localhost:4000";

export function useSocket(url: string = DEFAULT_URL): {
  socket: Socket | null;
  connected: boolean;
} {
  const socketRef = useRef<Socket | null>(null);
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const s = io(url, { transports: ["websocket"], autoConnect: true });
    socketRef.current = s;

    s.on("connect", () => setConnected(true));
    s.on("disconnect", () => setConnected(false));

    return () => {
      s.removeAllListeners();
      s.close();
      socketRef.current = null;
      setConnected(false);
    };
  }, [url]);

  return { socket: socketRef.current, connected };
}
