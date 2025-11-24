# Collaboration Architecture

## Overview

This document describes the technical architecture for adding real-time collaboration to the whiteboard application.

## Core Components

### 1. Operation-Based Sync (OT/CRDT)

**Choice: Operational Transformation (OT)**
- Simpler to implement than CRDT
- Better for ordered operations (drawing strokes)
- Proven in Google Docs, Figma

**Operation Types**:
```typescript
interface Operation {
  id: string;           // UUID
  type: 'create' | 'update' | 'delete' | 'move' | 'style';
  userId: string;
  timestamp: number;
  shapeId: string;
  data: any;
}
```

### 2. WebSocket Protocol

**Message Format**:
```json
{
  "type": "operation",
  "roomId": "abc123",
  "operation": {
    "id": "op-uuid",
    "type": "create",
    "userId": "user-123",
    "timestamp": 1697472000000,
    "shapeId": "shape-uuid",
    "data": { "tool": "rectangle", "points": [[100, 100], [200, 200]] }
  }
}
```

### 3. Client Architecture

**C++ Integration**:

```cpp
class CollaborationClient {
public:
    void connect(const std::string& server_url, const std::string& room_id);
    void disconnect();
    
    void send_operation(const Operation& op);
    void on_operation_received(std::function<void(const Operation&)> callback);
    
    void send_cursor_position(float x, float y);
    void on_cursor_update(std::function<void(const CursorUpdate&)> callback);
    
private:
    WebSocketClient ws_client_;
    std::string room_id_;
    std::string user_id_;
};
```

### 4. Server Architecture

**Node.js + Socket.io**:
```typescript
class CollaborationServer {
  private rooms: Map<string, Room>;
  
  handleConnection(socket: Socket) {
    socket.on('join-room', (roomId) => this.joinRoom(socket, roomId));
    socket.on('operation', (op) => this.broadcastOperation(socket, op));
    socket.on('cursor', (pos) => this.broadcastCursor(socket, pos));
  }
  
  broadcastOperation(socket: Socket, operation: Operation) {
    const room = this.rooms.get(operation.roomId);
    socket.to(operation.roomId).emit('operation', operation);
    this.saveOperation(operation);
  }
}
```

### 5. Data Flow

```
User Action → Local Apply → Send to Server → Broadcast to Others → Remote Apply
```

**Conflict Resolution**:
- Use vector clocks for ordering
- Last-write-wins for simple properties
- Merge strategies for complex operations

---

See COLLABORATION_ROADMAP.md for full implementation plan.
