#!/bin/bash
# Creates / updates the ROGUE.AI photo service in your GCP project: one Cloud
# Run service and one storage bucket (both rogue-ai-*). ./teardown.sh removes
# all of it.
#
# Usage:  PROJECT=your-gcp-project ./deploy.sh
# Uses YOUR Google Cloud project (gcloud auth login first; billing enabled).
# The first run saves PROJECT, the bucket name and two random keys in .keys
# (never committed); later runs read them from there.
set -euo pipefail
cd "$(dirname "$0")"
[ -f .keys ] && source .keys
PROJECT=${PROJECT:?"set PROJECT=your-gcp-project (see cloud/README.md)"}
REGION=${REGION:-us-central1}
BUCKET=${BUCKET:-rogue-ai-photos-$PROJECT}
SERVICE=${SERVICE:-rogue-ai-photos}

# Keys: UPLOAD_KEY goes into the firmware (src/secrets.h); ADMIN_KEY opens the
# delete page. Made once, kept in .keys with the project settings.
if [ -z "${UPLOAD_KEY:-}" ]; then
  UPLOAD_KEY=$(openssl rand -hex 16)
  ADMIN_KEY=$(openssl rand -hex 16)
fi
# GUEST_KEY: the guest app's key (it's in the join link guests get, so treat it
# as semi-public). ROTATE_GUEST=1 ./deploy.sh makes a new one -- e.g. after the
# party -- without touching the costume's or the scanners' key.
if [ -z "${GUEST_KEY:-}" ] || [ "${ROTATE_GUEST:-}" = 1 ]; then
  GUEST_KEY=$(openssl rand -hex 12)
fi
# The traitor game (cloud/service/main.py /game): picked automatically at GAME_START,
# the agent wins at GAME_END -- your local time, "YYYY-MM-DD HH:MM". Empty = start it
# from the admin page, no automatic end.
GAME_START=${GAME_START-"2026-10-30 21:00"}
GAME_END=${GAME_END-"2026-10-31 00:00"}
epoch() { [ -n "$1" ] && date -j -f "%Y-%m-%d %H:%M:%S" "$1:00" +%s || echo 0; }
cat > .keys <<KEYS
PROJECT=$PROJECT
REGION=$REGION
BUCKET=$BUCKET
SERVICE=$SERVICE
UPLOAD_KEY=$UPLOAD_KEY
ADMIN_KEY=$ADMIN_KEY
GUEST_KEY=$GUEST_KEY
GAME_START="$GAME_START"
GAME_END="$GAME_END"
KEYS

gcloud services enable run.googleapis.com cloudbuild.googleapis.com artifactregistry.googleapis.com \
  storage.googleapis.com vision.googleapis.com --project "$PROJECT" >/dev/null

if ! gcloud storage buckets describe gs://$BUCKET --project $PROJECT >/dev/null 2>&1; then
  gcloud storage buckets create gs://$BUCKET --project $PROJECT --location $REGION \
    --uniform-bucket-level-access --no-public-access-prevention
  # Photos are public (the gallery shows them straight from the bucket).
  gcloud storage buckets add-iam-policy-binding gs://$BUCKET \
    --member=allUsers --role=roles/storage.objectViewer >/dev/null
fi
SA="$(gcloud projects describe $PROJECT --format='value(projectNumber)')-compute@developer.gserviceaccount.com"
gcloud storage buckets add-iam-policy-binding gs://$BUCKET \
  --member="serviceAccount:$SA" --role=roles/storage.objectAdmin >/dev/null

gcloud run deploy $SERVICE --project $PROJECT --region $REGION --source service \
  --allow-unauthenticated --memory 512Mi --max-instances 3 \
  --set-env-vars "BUCKET=$BUCKET,UPLOAD_KEY=$UPLOAD_KEY,ADMIN_KEY=$ADMIN_KEY,GUEST_KEY=$GUEST_KEY,GAME_START=$(epoch "$GAME_START"),GAME_END=$(epoch "$GAME_END")" --quiet

URL=$(gcloud run services describe $SERVICE --project $PROJECT --region $REGION --format='value(status.url)')
echo
echo "Service: $URL"
echo "For src/secrets.h:  PHOTO_SERVICE_URL \"$URL\"   PHOTO_UPLOAD_KEY \"$UPLOAD_KEY\""
echo "Gallery feed (landing page CONFIG.feed): $URL/photos"
echo "Admin (private): $URL/admin?k=$ADMIN_KEY"
echo "Guest app join link (add &event=...&mid=...): lab7://join?service=$URL&key=$GUEST_KEY"
