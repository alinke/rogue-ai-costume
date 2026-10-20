#!/bin/bash
# Removes everything deploy.sh made: the Cloud Run service and the photo
# bucket (photos included). Save the photos first if you want them:
#   gcloud storage cp -r gs://<bucket from .keys> ~/Downloads/rogue-ai-photos
set -uo pipefail
cd "$(dirname "$0")"
[ -f .keys ] && source .keys
PROJECT=${PROJECT:?"set PROJECT=your-gcp-project"}
REGION=${REGION:-us-central1}
BUCKET=${BUCKET:-rogue-ai-photos-$PROJECT}
SERVICE=${SERVICE:-rogue-ai-photos}
read -r -p "Delete the $SERVICE service and ALL photos in gs://$BUCKET ($PROJECT)? [y/N] " ok
[ "$ok" = "y" ] || exit 1
gcloud run services delete $SERVICE --project $PROJECT --region $REGION --quiet
gcloud storage rm -r gs://$BUCKET --project $PROJECT
# The source upload bucket Cloud Run's --source deploy created (shared name, only
# rogue-ai's sources are removed).
gcloud storage rm -r "gs://run-sources-$PROJECT-$REGION/services/$SERVICE" 2>/dev/null
# The container image the deploy built (its repository was created for this).
gcloud artifacts docker images delete \
  "$REGION-docker.pkg.dev/$PROJECT/cloud-run-source-deploy/$SERVICE" --delete-tags --quiet 2>/dev/null
if [ -z "$(gcloud artifacts docker images list "$REGION-docker.pkg.dev/$PROJECT/cloud-run-source-deploy" --format='value(package)' 2>/dev/null)" ]; then
  gcloud artifacts repositories delete cloud-run-source-deploy --project $PROJECT --location $REGION --quiet 2>/dev/null
fi
# Face detection: deploy.sh enabled the Vision API. Leave it on if anything
# else in your project uses it: DISABLE_VISION=0 ./teardown.sh
[ "${DISABLE_VISION:-1}" = 1 ] && gcloud services disable vision.googleapis.com --project $PROJECT --quiet 2>/dev/null
echo "Left behind (should be nothing):"
gcloud run services list --project $PROJECT --region $REGION 2>/dev/null | grep "$SERVICE\|$BUCKET"
gcloud storage buckets list --project $PROJECT --format='value(name)' | grep "$SERVICE\|$BUCKET"
echo "Done. Remove the gallery block from the WordPress page too."
