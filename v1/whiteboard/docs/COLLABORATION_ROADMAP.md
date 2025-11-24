# Whiteboard Collaboration App Roadmap

## Executive Summary

The whiteboard application is a well-architected drawing and diagramming tool built on **NanoVG** (rendering) and **LunaSVG** (SVG support). It has a solid foundation with MVC architecture, comprehensive documentation, and a unique dual-format system (DDF + SVGShape). To transform it into a robust, professional collaborative app, we need to focus on **real-time collaboration**, **cloud infrastructure**, **performance optimization**, and **professional features**.

---

## Current State Assessment

### ✅ Strengths

1. **Solid Architecture**
   - Clean MVC pattern with observer-based updates
   - Separation of concerns (Model, View, Controller)
   - Well-documented codebase with 32+ documentation files
   - Comprehensive test coverage for core components

2. **Dual Format System**
   - **DDF (Diagram Definition Format)**: Complete document format with data binding, connectors, events
   - **SVGShape**: Reusable shape templates for library
   - Both render through same pipeline (LunaSVG → NanoVG)

3. **Rich Feature Set**
   - Drawing tools (pen, shapes, arrows, text)
   - SVG shape library with parametric shapes
   - Multi-page support with per-page undo/redo
   - Export to PNG/SVG/PDF
   - Auto-save and session restore
   - Clipboard operations (copy/paste/cut)
   - Grouping, alignment, layer ordering

4. **Professional UI**
   - Floating panels and toolbars
   - Context menus
   - Toast notifications
   - Keyboard shortcuts
   - Dark mode support
   - Resizable panels

### ⚠️ Gaps for Collaboration

1. **No Real-Time Collaboration**
   - Single-user only
   - No network layer
   - No conflict resolution
   - No presence indicators

2. **No Cloud Infrastructure**
   - Local file storage only
   - No user accounts
   - No project sharing
   - No version history

3. **Limited Collaboration Features**
   - No comments/annotations
   - No @mentions
   - No activity feed
   - No permissions system

4. **Performance Concerns**
   - No viewport culling (renders all shapes)
   - No lazy loading
   - Memory leaks in SVG rendering (noted in TODOs)
   - No shape caching

---

## Roadmap to Professional Collaboration App

### Phase 1: Foundation (2-3 months)

#### 1.1 Performance Optimization
**Priority: Critical**

- [ ] **Viewport Culling**
  - Only render shapes visible in viewport
  - Implement spatial indexing (R-tree or quadtree)
  - Target: Handle 10,000+ shapes smoothly

- [ ] **Shape Caching**
  - Cache rendered SVG shapes as textures
  - Invalidate cache on shape changes
  - Fix memory leak in `svg_renderer.cpp` (line 154)

- [ ] **Lazy Loading**
  - Load shape library on-demand
  - Paginate large documents
  - Stream DDF documents in chunks

- [ ] **Rendering Pipeline**
  - Implement dirty rectangle tracking
  - Batch similar shapes for GPU efficiency
  - Use instancing for repeated shapes

**Estimated Impact**: 10x performance improvement for large documents

#### 1.2 Data Model Refactoring
**Priority: High**

- [ ] **Operational Transformation (OT) / CRDT**
  - Choose between OT (simpler) or CRDT (more robust)
  - Implement operation log for all changes
  - Add operation serialization/deserialization
  - Design conflict resolution strategy

- [ ] **Document Versioning**
  - Add version numbers to all operations
  - Implement snapshot system (every N operations)
  - Store operation history for undo/redo across network

- [ ] **Unique IDs**
  - Replace array indices with UUIDs for all shapes
  - Implement ID generation strategy (UUID v4 or Snowflake)
  - Update all references to use IDs

**Estimated Impact**: Enables real-time collaboration

#### 1.3 Network Layer
**Priority: High**

- [ ] **WebSocket Server**
  - Choose framework (Node.js + Socket.io, Go + Gorilla, or Rust + Tokio)
  - Implement room-based architecture
  - Handle connection/disconnection gracefully
  - Add heartbeat/ping-pong for connection health

- [ ] **Protocol Design**
  - Define message format (JSON or MessagePack)
  - Operation types: create, update, delete, move, style
  - Presence updates: cursor position, selection, viewport
  - Metadata: user info, timestamps, operation IDs

- [ ] **Client Integration**
  - Add WebSocket client to C++ app (use libwebsockets or Boost.Beast)
  - Implement send/receive queues
  - Handle reconnection with exponential backoff
  - Sync state on connect/reconnect

**Estimated Impact**: Enables multi-user editing

---

### Phase 2: Core Collaboration (3-4 months)

#### 2.1 Real-Time Editing
**Priority: Critical**

- [ ] **Operation Broadcasting**
  - Send all local operations to server
  - Receive and apply remote operations
  - Handle operation ordering (vector clocks or Lamport timestamps)
  - Implement operation transformation/merging

- [ ] **Conflict Resolution**
  - Last-write-wins for simple properties
  - Merge strategies for complex operations
  - User notification for conflicts
  - Manual conflict resolution UI

- [ ] **Optimistic Updates**
  - Apply local operations immediately
  - Roll back on server rejection
  - Show "syncing" indicator
  - Queue operations during offline mode

**Estimated Impact**: Core collaboration functionality

#### 2.2 Presence & Awareness
**Priority: High**

- [ ] **User Cursors**
  - Show remote user cursors in real-time
  - Display user name/avatar near cursor
  - Smooth cursor interpolation
  - Hide cursors outside viewport

- [ ] **Selection Indicators**
  - Highlight shapes selected by other users
  - Show colored borders (per-user color)
  - Display user name on hover
  - Update in real-time

- [ ] **Viewport Indicators**
  - Show minimap with user viewports
  - Display user avatars at viewport positions
  - Click to follow user
  - Highlight active user

- [ ] **Activity Feed**
  - Show recent actions (shape created, moved, deleted)
  - Display user who performed action
  - Timestamp for each action
  - Filter by user or action type

**Estimated Impact**: Enhances collaboration awareness

#### 2.3 User Management
**Priority: High**

- [ ] **Authentication**
  - Email/password login
  - OAuth (Google, GitHub, Microsoft)
  - JWT tokens for session management
  - Refresh token rotation

- [ ] **User Profiles**
  - Display name, avatar, email
  - User preferences (theme, shortcuts)
  - Activity history
  - Profile editing

- [ ] **Permissions System**
  - Owner, Editor, Viewer roles
  - Per-document permissions
  - Share links with expiration
  - Public/private documents

**Estimated Impact**: Professional user management

---

### Phase 3: Cloud Infrastructure (2-3 months)

#### 3.1 Backend Services
**Priority: Critical**

- [ ] **Document Storage**
  - Choose database (PostgreSQL for metadata, S3 for documents)
  - Store DDF documents as JSON
  - Implement document versioning
  - Add full-text search (Elasticsearch)

- [ ] **API Server**
  - RESTful API for CRUD operations
  - GraphQL for complex queries (optional)
  - Rate limiting and throttling
  - API authentication (API keys + JWT)

- [ ] **File Storage**
  - Store images, exports, attachments
  - Use CDN for fast delivery (CloudFront, Cloudflare)
  - Implement upload limits and quotas
  - Generate thumbnails for previews

**Estimated Impact**: Scalable cloud infrastructure

#### 3.2 Collaboration Server
**Priority: High**

- [ ] **Room Management**
  - Create/join/leave rooms
  - Room capacity limits
  - Kick/ban users
  - Room persistence

- [ ] **Operation Log**
  - Store all operations in database
  - Implement log compaction (snapshots)
  - Replay operations for new joiners
  - Prune old operations

- [ ] **Presence Server**
  - Track active users per room
  - Broadcast presence updates
  - Handle user disconnections
  - Implement presence timeouts

**Estimated Impact**: Robust collaboration backend

#### 3.3 Deployment
**Priority: Medium**

- [ ] **Containerization**
  - Docker images for all services
  - Docker Compose for local development
  - Kubernetes manifests for production
  - Health checks and readiness probes

- [ ] **CI/CD Pipeline**
  - Automated testing (unit, integration, e2e)
  - Build and push Docker images
  - Deploy to staging/production
  - Rollback on failure

- [ ] **Monitoring**
  - Application metrics (Prometheus)
  - Logging (ELK stack or Loki)
  - Error tracking (Sentry)
  - Uptime monitoring (Pingdom)

**Estimated Impact**: Production-ready deployment

---

### Phase 4: Professional Features (3-4 months)

#### 4.1 Comments & Annotations
**Priority: High**

- [ ] **Comment System**
  - Add comments to shapes or canvas positions
  - Thread replies
  - @mentions with notifications
  - Resolve/unresolve comments

- [ ] **Annotations**
  - Sticky notes
  - Arrows pointing to shapes
  - Highlight areas
  - Drawing annotations

- [ ] **Notifications**
  - In-app notifications
  - Email notifications
  - Push notifications (web push)
  - Notification preferences

**Estimated Impact**: Enhanced collaboration communication

#### 4.2 Version History
**Priority: Medium**

- [ ] **Document Snapshots**
  - Auto-save snapshots every N minutes
  - Manual snapshot creation
  - Snapshot naming and descriptions
  - Snapshot comparison (diff view)

- [ ] **Version Browsing**
  - Timeline view of versions
  - Preview version without restoring
  - Restore to previous version
  - Branch from version (optional)

- [ ] **Change Tracking**
  - Show who changed what
  - Highlight changes between versions
  - Filter changes by user
  - Export change log

**Estimated Impact**: Professional version control

#### 4.3 Templates & Libraries
**Priority: Medium**

- [ ] **Template Gallery**
  - Pre-made templates (flowcharts, wireframes, org charts)
  - User-created templates
  - Template categories and tags
  - Template search and preview

- [ ] **Shape Libraries**
  - Expand built-in shape library
  - User-uploaded custom shapes
  - Team-shared shape libraries
  - Shape marketplace (optional)

- [ ] **Asset Management**
  - Upload images, icons, logos
  - Organize assets in folders
  - Search and filter assets
  - Asset usage tracking

**Estimated Impact**: Increased productivity

#### 4.4 Advanced Export
**Priority: Medium**

- [ ] **Export Formats**
  - High-res PNG (up to 8K)
  - Vector SVG with embedded fonts
  - Multi-page PDF
  - Animated GIF (for presentations)

- [ ] **Export Options**
  - Select specific pages/shapes
  - Background color/transparency
  - Include/exclude grid
  - Watermark for free tier

- [ ] **Presentation Mode**
  - Full-screen presentation
  - Navigate between pages
  - Laser pointer tool
  - Record presentation (optional)

**Estimated Impact**: Professional output

---

### Phase 5: Advanced Collaboration (2-3 months)

#### 5.1 Video & Voice
**Priority: Low**

- [ ] **Video Conferencing**
  - Integrate WebRTC (Jitsi, Agora, or Twilio)
  - In-app video calls
  - Screen sharing
  - Recording (optional)

- [ ] **Voice Chat**
  - Push-to-talk or always-on
  - Spatial audio (optional)
  - Mute/unmute controls
  - Audio quality settings

**Estimated Impact**: Enhanced remote collaboration

#### 5.2 AI Features
**Priority: Low**

- [ ] **Smart Shapes**
  - Auto-complete shapes
  - Suggest connections
  - Auto-layout diagrams
  - Shape recognition from sketches

- [ ] **AI Assistant**
  - Generate diagrams from text descriptions
  - Suggest improvements
  - Auto-generate documentation
  - Smart search

**Estimated Impact**: Cutting-edge features

#### 5.3 Integrations
**Priority: Low**

- [ ] **Third-Party Integrations**
  - Slack notifications
  - Jira/Trello for task management
  - Google Drive/Dropbox for storage
  - Figma/Sketch import

- [ ] **API & Webhooks**
  - Public API for developers
  - Webhooks for events
  - Zapier integration
  - Custom integrations

**Estimated Impact**: Ecosystem expansion

---

## Technical Architecture

### Recommended Stack

#### Frontend (C++ Desktop App)
- **Current**: NanoGUI + NanoVG + LunaSVG ✅
- **Add**: WebSocket client (libwebsockets or Boost.Beast)
- **Add**: JSON library (nlohmann/json) ✅ Already using
- **Add**: UUID library (stduuid or boost::uuid)

#### Backend
- **Language**: Node.js (TypeScript) or Go
  - Node.js: Faster development, rich ecosystem
  - Go: Better performance, easier deployment
- **WebSocket**: Socket.io (Node.js) or Gorilla (Go)
- **Database**: PostgreSQL (metadata) + Redis (cache)
- **Storage**: AWS S3 or MinIO (self-hosted)
- **Search**: Elasticsearch or Meilisearch

#### Infrastructure
- **Container**: Docker + Kubernetes
- **Load Balancer**: Nginx or Traefik
- **CDN**: CloudFront or Cloudflare
- **Monitoring**: Prometheus + Grafana
- **Logging**: ELK stack or Loki

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│                    CLIENT (C++ App)                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │   UI Layer   │  │  Model Layer │  │ Network Layer│ │
│  │  (NanoGUI)   │←→│ (Document)   │←→│ (WebSocket)  │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
                            ↕ WebSocket
┌─────────────────────────────────────────────────────────┐
│                 COLLABORATION SERVER                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ WebSocket    │  │ Room Manager │  │ Operation    │ │
│  │ Handler      │←→│              │←→│ Transformer  │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
                            ↕ HTTP/gRPC
┌─────────────────────────────────────────────────────────┐
│                      API SERVER                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ REST API     │  │ Auth Service │  │ File Storage │ │
│  │              │←→│              │←→│              │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
                            ↕
┌─────────────────────────────────────────────────────────┐
│                      DATABASE                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐ │
│  │ PostgreSQL   │  │    Redis     │  │      S3      │ │
│  │ (Metadata)   │  │   (Cache)    │  │  (Documents) │ │
│  └──────────────┘  └──────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────┘
```

---

## Implementation Priorities

### Must-Have (MVP for Collaboration)
1. ✅ Performance optimization (viewport culling, caching)
2. ✅ Operational transformation or CRDT
3. ✅ WebSocket server and client
4. ✅ Real-time operation broadcasting
5. ✅ User cursors and presence
6. ✅ Basic authentication
7. ✅ Document storage and sharing

### Should-Have (Professional Features)
1. Comments and annotations
2. Version history
3. Permissions system
4. Activity feed
5. Template gallery
6. Advanced export options

### Nice-to-Have (Competitive Advantage)
1. Video/voice chat
2. AI features
3. Third-party integrations
4. Mobile app (React Native or Flutter)
5. Offline mode with sync

---

## Immediate Next Steps

### Week 1-2: Performance Foundation
1. Implement viewport culling in `canvas_view.cpp`
2. Add spatial indexing (R-tree) for shape lookup
3. Fix SVG rendering memory leak
4. Add shape caching system

### Week 3-4: Data Model Preparation
1. Replace array indices with UUIDs
2. Implement operation log system
3. Add operation serialization
4. Design OT/CRDT strategy

### Week 5-6: Network Prototype
1. Set up Node.js WebSocket server
2. Implement basic room management
3. Add WebSocket client to C++ app
4. Test operation broadcasting

### Week 7-8: First Collaboration Demo
1. Implement cursor sharing
2. Add basic operation sync
3. Test with 2-3 users
4. Identify and fix issues

---

## Risk Assessment

### Technical Risks
- **OT/CRDT Complexity**: High learning curve, consider using library (Yjs, Automerge)
- **C++ WebSocket Integration**: Limited libraries, may need custom implementation
- **Performance at Scale**: Need load testing with 100+ concurrent users
- **Cross-Platform**: Ensure WebSocket works on Windows/Mac/Linux

### Business Risks
- **Competition**: Miro, Figma, Excalidraw are established
- **Monetization**: Need clear pricing strategy (freemium, team plans)
- **User Acquisition**: Requires marketing and community building
- **Infrastructure Costs**: Cloud hosting can be expensive at scale

### Mitigation Strategies
- Start with OT (simpler than CRDT), migrate later if needed
- Use proven WebSocket library (libwebsockets)
- Implement rate limiting and quotas early
- Consider self-hosted option for enterprises

---

## Success Metrics

### Performance
- Render 10,000+ shapes at 60 FPS
- Operation latency < 100ms (p95)
- Document load time < 2s (p95)
- Memory usage < 500MB for large documents

### Collaboration
- Support 50+ concurrent users per room
- Operation sync latency < 200ms (p95)
- Conflict rate < 1% of operations
- 99.9% uptime for collaboration server

### User Experience
- Onboarding completion rate > 80%
- Daily active users (DAU) growth
- Average session duration > 15 minutes
- User retention rate > 40% (30-day)

---

## Conclusion

The whiteboard app has a **solid foundation** with clean architecture, comprehensive documentation, and rich features. To become a professional collaboration app, focus on:

1. **Performance optimization** (viewport culling, caching)
2. **Real-time collaboration** (OT/CRDT, WebSocket)
3. **Cloud infrastructure** (backend services, deployment)
4. **Professional features** (comments, version history, templates)

**Estimated Timeline**: 12-15 months for full implementation
**Team Size**: 3-5 developers (1 C++, 2 backend, 1 frontend, 1 DevOps)
**Budget**: $200K-$400K (salaries + infrastructure)

The unique dual-format system (DDF + SVGShape) is a **competitive advantage** - it enables both simple drawing and complex data-driven diagrams. With proper execution, this can compete with Miro and Figma in specific niches (technical diagrams, data visualization).

---

**Last Updated**: October 2025
**Author**: Kiro AI Assistant
**Status**: Draft for Review
